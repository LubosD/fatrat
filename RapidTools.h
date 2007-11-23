#ifndef RAPIDTOOLS_H
#define RAPIDTOOLS_H
#include <QWidget>
#include <QHttp>
#include <QBuffer>
#include <QMap>
#include "ui_RapidTools.h"

class RapidTools : public QWidget, Ui_RapidTools
{
Q_OBJECT
public:
	RapidTools();
	static QWidget* create() { return new RapidTools; }
public slots:
	void checkRShareLinks();
	void downloadRShareLinks();
	void doneRShare(bool error);
	
	void decodeRSafeLinks();
	void downloadRSafeLinks();
	void doneRSafe(int r, bool error);
	void doneRSafe(bool);
	
	void extractRFLinks();
	void downloadRFLinks();
	void doneRF(bool error);
private:
	QHttp *m_httpRShare, *m_httpRSafe, *m_httpRF;
	QBuffer *m_bufRShare, *m_bufRF;
	
	QMap<unsigned long, QString> m_mapRShare;
	QString m_strRShareWorking;
	
	QMap<int, QString> m_listRSafeSrc;
};

#endif
