#ifndef NETIFACE_H
#define NETIFACE_H
#include <QString>
#include <QPair>

QString getRoutingInterface4();
QPair<qint64, qint64> getInterfaceStats(QString iface);

#endif
