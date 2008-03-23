#ifndef DBUSIMPL_H
#define DBUSIMPL_H
#include <QObject>
#include <QStringList>

class DbusImpl : public QObject
{
Q_OBJECT
public:
	DbusImpl();
	static DbusImpl* instance() { return m_instance; }
public slots:
	void addTransfers(QString uris);
	QString addTransfersNonInteractive(QString uris, QString target, QString className, int queueID);
	QStringList getQueues();
private:
	static DbusImpl* m_instance;
};

#endif
