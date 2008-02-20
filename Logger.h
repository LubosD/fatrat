#ifndef LOGGER_H
#define LOGGER_H
#include <QObject>
#include <QString>
#include <QReadWriteLock>

class Logger : public QObject
{
Q_OBJECT
public:
	void enterLogMessage(QString msg);
	void enterLogMessage(QString sender, QString msg);
	QString logContents() const;
	static Logger* global() { return &m_global; }
signals:
	void logMessage(QString msg);
private:
	QString m_strLog;
	mutable QReadWriteLock m_lock;
	static Logger m_global;
};

#endif
