#ifndef RSSFETCHER_H
#define RSSFETCHER_H
#include <QObject>
#include <QTimer>
#include <QHttp>

class RssFetcher : public QObject
{
Q_OBJECT
public:
	RssFetcher();
public slots:
	void refresh();
private:
	QTimer m_timer;
	QHttp* m_http;
};

#endif
