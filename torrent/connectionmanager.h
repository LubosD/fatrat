/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

class PeerWireClient;

#include <QByteArray>
#include <QSet>

class ConnectionManager
{
public:
    static ConnectionManager *instance();

    bool canAddConnection() const;
    void addConnection(PeerWireClient *connection);
    void removeConnection(PeerWireClient *connection);
    int maxConnections() const;
    void setMaxConnections(int newmax);
    QByteArray clientId() const;

 private:
    QSet<PeerWireClient *> connections;
    mutable QByteArray id;
};

#endif
