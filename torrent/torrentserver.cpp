/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#include "connectionmanager.h"
#include "peerwireclient.h"
#include "ratecontroller.h"
#include "torrentclient.h"
#include "torrentserver.h"

Q_GLOBAL_STATIC(TorrentServer, torrentServer)

TorrentServer *TorrentServer::instance()
{
    return torrentServer();
}

void TorrentServer::addClient(TorrentClient *client)
{
    clients << client;
}

void TorrentServer::removeClient(TorrentClient *client)
{
    clients.removeAll(client);
}

void TorrentServer::incomingConnection(int socketDescriptor)
{
    PeerWireClient *client = new PeerWireClient(ConnectionManager::instance()->clientId(), this);
    if (clients.isEmpty())
        client->abort();

    if (ConnectionManager::instance()->canAddConnection()) {
        if (client->setSocketDescriptor(socketDescriptor)) {
            connect(client, SIGNAL(infoHashReceived(const QByteArray &)),
                    this, SLOT(processInfoHash(const QByteArray &)));
            connect(client, SIGNAL(error(QAbstractSocket::SocketError)),
                    this, SLOT(removeClient()));
           // RateController::instance(clients.first())->addSocket(client);
            ConnectionManager::instance()->addConnection(client);
            if (clients.size() == 1) {
                client->disconnect(client, 0, this, 0);
                clients.first()->setupIncomingConnection(client);
            }
            return;
        }
    }

    delete client;
}

void TorrentServer::removeClient()
{
    PeerWireClient *peer = qobject_cast<PeerWireClient *>(sender());
    peer->deleteLater();
    RateController::removeSocketGlobal(peer);
    ConnectionManager::instance()->removeConnection(peer);
}

void TorrentServer::processInfoHash(const QByteArray &infoHash)
{
    PeerWireClient *peer = qobject_cast<PeerWireClient *>(sender());
    foreach (TorrentClient *client, clients) {
        if (client->state() >= TorrentClient::Searching && client->infoHash() == infoHash) {
            peer->disconnect(peer, 0, this, 0);
            client->setupIncomingConnection(peer);
            return;
        }
    }
    removeClient();
}
