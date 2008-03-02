#ifndef DBUSIMPL_H
#define DBUSIMPL_H
#include <QObject>
#include <QStringList>

class DbusImpl : public QObject
{
Q_OBJECT
public slots:
	void addTransfers(QString uris);
	static void addTransfersNonInteractive(QString uris, QString target, QString className, int queueID);
	QStringList getQueues();
};

#endif
