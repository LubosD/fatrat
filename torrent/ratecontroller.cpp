/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#include "peerwireclient.h"
#include "ratecontroller.h"

#include <QtCore>
#include <QReadWriteLock>
#include <QApplication>

//Q_GLOBAL_STATIC(RateController, rateController)

static QReadWriteLock m_lock;
static QSet<RateController*> m_instances;

RateController::RateController(TorrentClient* client)
        : /*QObject(client),*/ m_client(client), upLimit(1024*1024), downLimit(1024*1024), transferScheduled(false)
{
	moveToThread(QApplication::instance()->thread());
	m_lock.lockForWrite();
	m_instances << this;
	m_lock.unlock();
}

RateController::~RateController()
{
	m_lock.lockForWrite();
	m_instances.remove(this);
	m_lock.unlock();
}

RateController *RateController::instance(TorrentClient* ifor)
{
	//return rateController();
	m_lock.lockForRead();
	RateController* rc = 0;
	
	foreach(RateController* r,m_instances)
	{
		if(r->m_client == ifor)
		{
			rc = r;
			break;
		}
	}
	
	m_lock.unlock();
	
	if(rc)
		return rc;
	else
		return new RateController(ifor);
}

void RateController::removeSocketGlobal(PeerWireClient *socket)
{
	//instance(0)->removeSocket(socket);
	
	m_lock.lockForRead();
	foreach(RateController* r,m_instances)
	{
		if(r->removeSocket(socket))
			break;
	}
	m_lock.unlock();
	
}

void RateController::addSocket(PeerWireClient *socket)
{
    connect(socket, SIGNAL(readyToTransfer()), this, SLOT(scheduleTransfer()));
    socket->setReadBufferSize(downLimit * 4);
    sockets << socket;
    scheduleTransfer();
}

bool RateController::removeSocket(PeerWireClient *socket)
{
    if(sockets.remove(socket))
    {
    	disconnect(socket, SIGNAL(readyToTransfer()), this, SLOT(scheduleTransfer()));
    	socket->setReadBufferSize(0);
	return true;
    }
    return false;
}

void RateController::setDownloadLimit(int bytesPerSecond)
{
    downLimit = bytesPerSecond;
    foreach (PeerWireClient *socket, sockets)
        socket->setReadBufferSize(downLimit * 4);
}

void RateController::scheduleTransfer()
{
    if (transferScheduled)
        return;
    transferScheduled = true;
    QTimer::singleShot(50, this, SLOT(transfer()));
}

void RateController::transfer()
{
    transferScheduled = false;
    if (sockets.isEmpty())
        return;

    int msecs = 1000;
    if (!stopWatch.isNull())
        msecs = qMin(msecs, stopWatch.elapsed());

    qint64 bytesToWrite = (upLimit * msecs) / 1000;
    qint64 bytesToRead = (downLimit * msecs) / 1000;
    if (bytesToWrite == 0 && bytesToRead == 0) {
        scheduleTransfer();
        return;
    }

    QSet<PeerWireClient *> pendingSockets;
    foreach (PeerWireClient *client, sockets) {
        if (client->canTransferMore())
            pendingSockets << client;
    }
    if (pendingSockets.isEmpty())
        return;

    stopWatch.start();

    bool canTransferMore;
    do {
        canTransferMore = false;
        qint64 writeChunk = qMax<qint64>(1, bytesToWrite / pendingSockets.size());
        qint64 readChunk = qMax<qint64>(1, bytesToRead / pendingSockets.size());

        QSetIterator<PeerWireClient *> it(pendingSockets);
        while (it.hasNext() && (bytesToWrite > 0 || bytesToRead > 0)) {
            PeerWireClient *socket = it.next();
            if (socket->state() != QAbstractSocket::ConnectedState) {
                pendingSockets.remove(socket);
                continue;
            }

            bool dataTransferred = false;
            qint64 available = qMin<qint64>(socket->socketBytesAvailable(), readChunk);
            if (available > 0) {
                qint64 readBytes = socket->readFromSocket(qMin<qint64>(available, bytesToRead));
                if (readBytes > 0) {
                    bytesToRead -= readBytes;
                    dataTransferred = true;
                }
            }

            if (upLimit * 2 > socket->bytesToWrite()) {
                qint64 chunkSize = qMin<qint64>(writeChunk, bytesToWrite);
                qint64 toWrite = qMin(upLimit * 2 - socket->bytesToWrite(), chunkSize);
                if (toWrite > 0) {
                    qint64 writtenBytes = socket->writeToSocket(toWrite);
                    if (writtenBytes > 0) {
                        bytesToWrite -= writtenBytes;
                        dataTransferred = true;
                    }
                }
            }

            if (dataTransferred && socket->canTransferMore())
                canTransferMore = true;
            else
                pendingSockets.remove(socket);
        }
    } while (canTransferMore && (bytesToWrite > 0 || bytesToRead > 0) && !pendingSockets.isEmpty());

    if (canTransferMore || bytesToWrite == 0 || bytesToRead == 0)
        scheduleTransfer();
}
