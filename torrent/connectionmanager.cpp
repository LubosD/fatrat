/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#include "connectionmanager.h"

#include <QByteArray>
#include <QDateTime>

static int MaxConnections = INT_MAX;

Q_GLOBAL_STATIC(ConnectionManager, connectionManager)

ConnectionManager *ConnectionManager::instance()
{
    return connectionManager();
}

bool ConnectionManager::canAddConnection() const
{
    return (connections.size() < MaxConnections);
}

void ConnectionManager::addConnection(PeerWireClient *client)
{
    connections << client;
}

void ConnectionManager::removeConnection(PeerWireClient *client)
{
    connections.remove(client);
}

int ConnectionManager::maxConnections() const
{
    return MaxConnections;
}

void ConnectionManager::setMaxConnections(int newmax)
{
	MaxConnections = (newmax) ? newmax : INT_MAX;
}

QByteArray ConnectionManager::clientId() const
{
    if (id.isEmpty()) {
        // Generate peer id
        int startupTime = int(QDateTime::currentDateTime().toTime_t());

        QString s;
        s.sprintf("-QT%06x", QT_VERSION);
        id += s.toLatin1();
        id += QByteArray::number(startupTime, 16);
        id += QByteArray(20 - id.size(), '-');
    }
    return id;
}
