#include "LimitedSocket.h"

#include <QtDebug>
#include <QHostAddress>
#include <QNetworkProxy>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <QTcpSocket>

using namespace std;

const int SPEED_SAMPLES = 7;

LimitedSocket::LimitedSocket()
	: m_bAbort(false), m_nTransfered(0), m_nToTransfer(0),
	m_nResume(0), m_nSegmentStart(-1), m_nSegmentEnd(-1), m_nSpeedLimit(0)
{
}

LimitedSocket::~LimitedSocket()
{
	wait();
}

QString LimitedSocket::getErrorString(QAbstractSocket::SocketError err)
{
	switch(err)
	{
	case QAbstractSocket::ConnectionRefusedError:
		return tr("Connection refused");
	case QAbstractSocket::RemoteHostClosedError:
		return tr("Connection lost");
	case QAbstractSocket::HostNotFoundError:
		return tr("Host not found");
	case QAbstractSocket::SocketAccessError:
		return tr("Access denied");
	case QAbstractSocket::SocketResourceError:
		return tr("Socket resource error");
	case QAbstractSocket::SocketTimeoutError:
		return tr("Timeout");
	case QAbstractSocket::DatagramTooLargeError:
		return tr("Datagram too large");
	case QAbstractSocket::NetworkError:
		return tr("Network error");
	case QAbstractSocket::AddressInUseError:
		return tr("Address is already in use");
	case QAbstractSocket::SocketAddressNotAvailableError:
		return tr("Address is not available");
	case QAbstractSocket::UnsupportedSocketOperationError:
		return tr("Unsupported operation");
	case QAbstractSocket::ProxyAuthenticationRequiredError:
		return tr("The proxy requires authentication");
	default:
		return tr("Unknown error");
	}
}

bool LimitedSocket::bindSocket(QAbstractSocket* sock, QHostAddress addr)
{
	if(addr.isNull())
		return true;
	
	int error;
	
	if(addr.protocol() == QAbstractSocket::IPv4Protocol)
	{
		sockaddr_in in;
		
		memset(&in, 0, sizeof(in));
		in.sin_addr.s_addr = addr.toIPv4Address();
		in.sin_family = AF_INET;
		
		error = ::bind(sock->socketDescriptor(), (sockaddr*) &in, sizeof(in));
	}
	else
	{
		sockaddr_in6 in6;
		
		Q_IPV6ADDR a = addr.toIPv6Address();
		memset(&in6, 0, sizeof(in6));
		memcpy(in6.sin6_addr.s6_addr, a.c, 16);
		in6.sin6_family = AF_INET6;
		
		error = ::bind(sock->socketDescriptor(), (sockaddr*) &in6, sizeof(in6));
	}
	
	return !error;
}

bool LimitedSocket::open(QString file, bool bUpload)
{
	QIODevice::OpenMode mode;
	
	m_bUpload = bUpload;
	
	if(bUpload)
		mode = QIODevice::ReadOnly;
	else
		mode = (m_nSegmentStart < 0) ? QIODevice::Append : QIODevice::WriteOnly;
	
	m_file.setFileName(file);
	if(!m_file.open(mode))
	{
		qDebug() << "Failed to open file" << file;
		m_strError = tr("Failed to open file");
		return false;
	}
	
	if(m_nSegmentStart > 0)
		m_file.seek(m_nSegmentStart);
	
	return true;
}

void LimitedSocket::destroy()
{
	m_bAbort = true;
	
	if(isRunning())
		connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
	else
		delete this;
}

QByteArray LimitedSocket::readLine()
{
	while(true)
	{
		QByteArray array = getRemote()->readLine();
		if(!array.isEmpty())
			return array;
		else
		{
			if(!getRemote()->waitForReadyRead(10000))
				return QByteArray();
		}
	}
}

bool LimitedSocket::writeCycle()
{
	int toRead;
	QByteArray buffer;
	QIODevice* pRemote = getRemote();
	
	if(m_timer.isNull())
	{
		m_prevBytes = 0;
		m_timer.start();
	}
	
	toRead = qMin<qint64>(1024, m_nToTransfer-m_nTransfered);
	buffer = m_file.read(toRead);
	
	if(buffer.size() < toRead)
	{
		m_strError = tr("Error reading file");
		return false;
	}
	
	pRemote->write(buffer);
	if(!pRemote->waitForBytesWritten(10000))
		return false;
	
	m_nTransfered += toRead;
	
	QTime now = QTime::currentTime();
	int msecs;
	qint64 bytes = m_nTransfered - m_prevBytes;
	
	if((msecs = m_timer.elapsed()) > 1000)
	{
		m_statsLock.lockForWrite();
		m_stats << QPair<int,qulonglong>(m_timer.restart(), bytes);
		if(m_stats.size() > SPEED_SAMPLES)
			m_stats.removeFirst();
		m_statsLock.unlock();
		m_prevBytes = m_nTransfered;
	}
	
	if(m_nSpeedLimit > 0 && bytes)
	{
		int sleeptime = bytes/(m_nSpeedLimit/1000) - msecs;
		if(sleeptime > 0)
			msleep(sleeptime*2);
	}
	
	return true;
}

bool LimitedSocket::readCycle()
{
	QByteArray buffer;
	bool bProblem = false;
	
	QIODevice* pRemote = getRemote();

	if(m_timer.isNull())
	{
		m_prevBytes = 0;
		m_timer.start();
	}
	
	int limit = m_nSpeedLimit;

	do
	{
		QByteArray buf;
		qint64 available;
		
		available = pRemote->bytesAvailable();
		if(!available && !pRemote->waitForReadyRead(10000))
		{
			bProblem = true;
			break;
		}
		
		qint64 toread = (limit) ? limit/2 : available;
		
		if(m_nToTransfer && m_nToTransfer-m_nTransfered < toread)
			toread = m_nToTransfer-m_nTransfered;
		
		buf = pRemote->read(toread);
		buffer += buf;
		m_nTransfered += buf.size();
		
		if(m_nTransfered >= m_nToTransfer && m_nToTransfer)
			break;
	}
	while(buffer.size() < 1024);

	m_file.write(buffer);

	if(bProblem)
	{
		/*if(m_pSocket->state() == QAbstractSocket::ConnectedState) // timeout
			m_strError = tr ( "Timeout" );
		else if(m_pSocket->error() == QAbstractSocket::RemoteHostClosedError)
		{
			if(m_nToTransfer > m_nTransfered && m_nToTransfer)
			{
				qDebug() << "Remote host closed the connection";
				m_strError = tr ( "Connection lost" );
			}
		}
		else
		{
			m_strError = m_pSocket->errorString();
			if ( m_strError.isEmpty() )
			{
				qDebug() << "Connection lost!";
				m_strError = tr ( "Connection lost" );
			}
		}*/
	}
	else if(!m_bAbort)
	{
		QTime now = QTime::currentTime();
		qulonglong bnow = m_nTransfered;

		int msecs;
		qulonglong bytes = bnow - m_prevBytes;

		if((msecs = m_timer.elapsed()) > 1000)
		{
			m_statsLock.lockForWrite();
			m_stats << QPair<int,qulonglong> ( m_timer.restart(), bytes );
			if(m_stats.size() > SPEED_SAMPLES)
				m_stats.removeFirst();
			m_statsLock.unlock();

			m_prevBytes = bnow;
		}

		if(m_nSpeedLimit > 0 && bytes)
		{
			int sleeptime = bytes/ ( limit/1000 ) - msecs;
			if(sleeptime > 0)
				msleep(sleeptime*2);
		}
	}

	return !bProblem;
}


int LimitedSocket::speed() const
{
	int msecs = 0;
	qulonglong bytes = 0;
	
	m_statsLock.lockForRead();
	for(int i=0;i<m_stats.size();i++)
	{
		msecs += m_stats[i].first;
		bytes += m_stats[i].second;
	}
	m_statsLock.unlock();
	
	if(!msecs)
		return 0;
	else
		return 1000*bytes/msecs;
}

void LimitedSocket::doClose(QTcpSocket** sock)
{
	if(!*sock)
		return;
	(*sock)->close();
	
	QAbstractSocket::SocketState state = (*sock)->state();
	if(state == QAbstractSocket::ConnectedState || state == QAbstractSocket::ClosingState)
		(*sock)->waitForDisconnected(5000);
	delete (*sock);
	(*sock) = 0;
}

/*void LimitedSocket::reset()
{
	terminate();
	delete m_pSocket;
	m_pSocket = 0;
	m_nTransfered = m_nToTransfer = 0;
	m_stats.clear();
	m_timer = QTime();
	m_file.close();
}*/

