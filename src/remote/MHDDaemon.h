#ifndef FR_MHDDAEMON_H
#define FR_MHDDAEMON_H

#include <QObject>
#include <microhttpd.h>
#include <QRegExp>
#include <QList>
#include <QByteArray>
#include "MHDService.h"

class MHDDaemon : public QObject
{
Q_OBJECT
public:
	MHDDaemon();
	virtual ~MHDDaemon();

	void setPassword(QString pass);

	void start(int port);
	void startSSL(int port);
	void stop();

	// Takes ownership of the handler!
	void addHandler(QRegExp regexp, MHDService* handler);
protected:
	static int process(void* t, struct MHD_Connection* conn, const char* url, const char* method, const char* version, const char* upload_data, size_t* upload_data_size, void** ptr);
	static void finished(void* t, struct MHD_Connection* conn, void** ptr, enum MHD_RequestTerminationCode toe); 
private:
	struct Handler
	{
		QRegExp regexp;
		MHDService* service;
	};
	struct State
	{
		Handler* handler;
		QByteArray postData;
	};

	MHD_Daemon* m_daemon;
	QList<Handler> m_handlers;
	QString m_strPassword;
};


#endif

