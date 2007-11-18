#include "SftpClient.h"
#include <QFileInfo>
#include <QtDebug>
#include <fcntl.h>

SftpEngine::SftpEngine(QUrl url)
	: m_url(url), m_pFileDevice(0), m_pSocket(0)
{
	m_pSession = libssh2_session_init_ex(0, 0, 0, this);
}

SftpEngine::~SftpEngine()
{
	libssh2_session_free(m_pSession);
	delete m_pSocket;
}

void SftpEngine::request(QString localfile, bool bUpload, int)
{
	if(LimitedSocket::open(localfile, bUpload))
	{
		if(!bUpload)
		{
			QString path = m_url.path();
			int pos = path.lastIndexOf('/');
			
			if(pos != -1)
				m_strName = path.mid(pos+1);
			else
				m_strName = path;
			
			m_nResume = m_file.pos();
		}
		else
		{
			QFileInfo finfo(localfile);
			m_strName = finfo.fileName();
			m_nToTransfer = finfo.size();
		}
		start();
	}
	else
	{
		m_strError = tr("Failed to open file");
		emit finished(true);
	}
}

static LIBSSH2_USERAUTH_KBDINT_RESPONSE_FUNC(kbInteractiveCallback)
{
	if (num_prompts != 1 || prompts[0].echo)
	{
        	for(int i=0;i < num_prompts;i++)
			responses[i].length = 0;
	}
	else
	{
		SftpEngine* This = (SftpEngine*) *abstract;
		QByteArray pwd = This->m_url.password().toUtf8();
		int len = pwd.size();
		
		responses[0].text = (char*) malloc(len);
		memcpy(responses[0].text, pwd.constData(), len);
		responses[0].length = len;
	}
}

qint64 SftpEngine::querySize(LIBSSH2_SFTP* sftp)
{
	LIBSSH2_SFTP_ATTRIBUTES attr;
	QByteArray path = m_url.path().toUtf8();
	int res = 0;
	
	res = libssh2_sftp_stat(sftp, path.constData(), &attr);
	
	if(res < 0)
		return 0;
	else
		return attr.filesize;
}

void SftpEngine::run()
{
	LIBSSH2_SFTP* sftp = 0;
	
	try
	{
		m_pSocket = new QTcpSocket;
		m_pSocket->connectToHost(m_url.host(), m_url.port(22));
		
		emit statusMessage("Connecting");
		if(!m_pSocket->waitForConnected())
		{
			qDebug() << "Failed to connect" << m_pSocket->errorString();
			throw m_pSocket->errorString();
		}
		
		int sock = m_pSocket->socketDescriptor();
		int mode = fcntl(sock,F_GETFL);
		fcntl(sock,F_SETFL,mode & ~O_NDELAY);
		
		if(libssh2_session_startup(m_pSession, sock) < 0)
			throw tr("libssh2_session_startup failed");
		
		QByteArray data = m_url.userName().toUtf8();
		
		if(libssh2_userauth_keyboard_interactive(m_pSession, data.constData(), kbInteractiveCallback) < 0)
			throw tr("SSH authentication failed");
		
		emit statusMessage("Authenticated");
		
		sftp = libssh2_sftp_init(m_pSession);
		if(sftp == 0)
			throw tr("Failed to open the SFTP subsystem");
		
		data = m_url.path().toUtf8();
		
		qint64 size = querySize(sftp);
		if(!m_bUpload)
		{
			if(m_nSegmentEnd < 0)
			{
				if(size > 0)
					m_nToTransfer = size - m_nResume;
				else
					m_nToTransfer = 0;
			}
			else
				m_nToTransfer = m_nSegmentEnd - m_nResume;
		}
		else
		{
			m_nResume = size;
			m_nToTransfer -= m_nResume;
			
			data += '/';
			data += m_strName;
		}
		
		emit receivedSize(size);
		
		emit statusMessage("Opening file");
		
		LIBSSH2_SFTP_HANDLE* handle;
		handle = libssh2_sftp_open(sftp, data.constData(), (m_bUpload) ? LIBSSH2_FXF_WRITE|LIBSSH2_FXF_CREAT : LIBSSH2_FXF_READ, 0666);
		if(handle == 0)
			throw tr("Failed to open remote file");
		
		m_pFileDevice = new SftpFile(handle, size, 0);
		m_pFileDevice->open( (!m_bUpload) ? QIODevice::ReadOnly : QIODevice::WriteOnly );
		m_pFileDevice->seek(m_nResume);
		
		emit statusMessage("Transferring");
		
		while(!m_bAbort && m_nTransfered < m_nToTransfer)
		{
			bool success;
			
			if(!m_bUpload)
				success = readCycle();
			else
				success = writeCycle();
			
			if(!success)
				throw tr("Error reading/writing data");
		}
		
		m_file.close();
		
		if(!m_bAbort)
			emit finished(false);
	}
	catch(const QString& msg)
	{
		qDebug() << "Exception:" << msg;
		
		char* err;
		libssh2_session_last_error(m_pSession, &err, 0, 0);
		
		qDebug () << err;
		
		if(!m_bAbort)
		{
			m_strError = msg.trimmed();
			emit finished(true);
		}
	}
	
	if(m_pFileDevice != 0)
		delete m_pFileDevice;
	if(sftp != 0)
		libssh2_sftp_shutdown(sftp);
	
	if(m_pSocket)
		doClose(&m_pSocket);
}

SftpFile::SftpFile(LIBSSH2_SFTP_HANDLE* handle, qint64 mysize, QObject* parent)
	: QIODevice(parent), m_handle(handle), m_nSize(mysize)
{
}

void SftpFile::close()
{
	if(m_handle != 0)
		libssh2_sftp_close(m_handle);
}

bool SftpFile::open(OpenMode mode)
{
	setOpenMode(mode);
	return true;
}

bool SftpFile::atEnd() const
{
	return pos() == m_nSize;
}

qint64 SftpFile::bytesAvailable() const
{
	return 4096;
}

qint64 SftpFile::pos() const
{
	return libssh2_sftp_tell(m_handle);
}

bool SftpFile::reset()
{
	return seek(0);
}

bool SftpFile::seek(qint64 pos)
{
	if(pos >= 0 && pos < m_nSize)
	{
		libssh2_sftp_seek(m_handle, pos);
		return true;
	}
	else
		return false;
}

qint64 SftpFile::size() const
{
	return m_nSize;
}

qint64 SftpFile::readData(char* data, qint64 maxSize)
{
	return libssh2_sftp_read(m_handle, data, maxSize);
}

qint64 SftpFile::writeData(const char* data, qint64 maxSize)
{
	return libssh2_sftp_write(m_handle, data, maxSize);
}
