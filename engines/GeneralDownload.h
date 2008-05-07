#ifndef _GENERALDOWNLOAD_H
#define _GENERALDOWNLOAD_H

#include "Transfer.h"
#include "LimitedSocket.h"
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttp>
#include <QPair>
#include <QTime>
#include <QMap>
#include <QReadWriteLock>
#include <QDialog>
#include <QUuid>

class GeneralDownload : public Transfer
{
Q_OBJECT
public:
	GeneralDownload(bool local = false);
	virtual ~GeneralDownload();
	
	virtual void changeActive(bool);
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	virtual QString name() const;
	virtual QString myClass() const { return "GeneralDownload"; }
	virtual QString message() const { return m_strMessage; }
	
	virtual void init(QString uri,QString dest);
	virtual void setObject(QString target);
	void init2(QString uri,QString dest);
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual QString object() const { return m_dir.path(); }
	
	virtual void setSpeedLimits(int down,int up);
	// options of a particular download
	virtual WidgetHostChild* createOptionsWidget(QWidget* w);
	virtual void fillContextMenu(QMenu& menu);
	
	// global download options
	//static WidgetHostChild* createSettingsWidget(QWidget* w,QIcon&);
	static QDialog* createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers);
	
	static int acceptable(QString uri, bool);
	static Transfer* createInstance() { return new GeneralDownload; }
	static void globalInit();
	
	QString filePath() const { return m_dir.filePath(name()); }
	void setTargetName(QString strName);
	
	static const char* getDescription();
private slots:
	void requestFinished(bool error);
	void responseSizeReceived(qint64 totalsize);
	void redirected(QString newurl);
	void changeMessage(QString msg) { m_strMessage = msg; }
	void renamed(QString dispName);
	
	void switchMirror();
	void computeHash();
	void connectSignals();
private:
	void startHttp(QUrl url, QUrl referrer = QUrl());
	void startFtp(QUrl url);
	void startSftp(QUrl url);
	void generateName();
protected:
	QUrl m_urlLast;
	QDir m_dir;
	qulonglong m_nTotal,m_nStart;
	
	QString m_strMessage, m_strFile;
	bool m_bAutoName;
	
	LimitedSocket* m_engine;
	
	struct UrlObject
	{
		QUrl url;
		QString strReferrer, strBindAddress;
		FtpMode ftpMode;
		QUuid proxy;
	};
	QList<UrlObject> m_urls;
	int m_nUrl;
	QMap<QString, QString> m_cookies;
	
	friend class HttpOptsWidget;
	friend class HttpUrlOptsDlg;
};

#endif

