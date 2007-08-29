/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#ifndef RATECONTROLLER_H
#define RATECONTROLLER_H

#include <QObject>
#include <QSet>
#include <QTime>
#include "torrentclient.h"

class PeerWireClient;

class RateController : public QObject
{
    Q_OBJECT

public:
    RateController(TorrentClient* client = 0);
    virtual ~RateController();
    static RateController *instance(TorrentClient* ifor);

    void addSocket(PeerWireClient *socket);
    bool removeSocket(PeerWireClient *socket);
    static void removeSocketGlobal(PeerWireClient *socket);

    inline int uploadLimit() const { return upLimit; }
    inline int downloadLimit() const { return downLimit; }
    inline void setUploadLimit(int bytesPerSecond) { upLimit = bytesPerSecond; }
    void setDownloadLimit(int bytesPerSecond);

public slots:
    void transfer();
    void scheduleTransfer();

private:
    QTime stopWatch;
    QSet<PeerWireClient *> sockets;
    TorrentClient* m_client;
    int upLimit;
    int downLimit;
    bool transferScheduled;
};

#endif
