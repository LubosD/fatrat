#ifndef DBUSIMPL_H
#define DBUSIMPL_H
#include <QObject>
#include <QStringList>

class DbusImpl : public QObject
{
Q_OBJECT
public slots:
	void addTransfers(QString uris);
	void addTransfersNonInteractive(QString uris, QString target, QString className, QString classType, int queueID);
	QStringList getQueues();
};

#endif
