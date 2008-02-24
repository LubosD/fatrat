#ifndef RAPIDSHAREUPLOAD_H
#include "Transfer.h"
#include "HttpClient.h"
#include "WidgetHostChild.h"
#include "ui_RapidshareOptsForm.h"

class QBuffer;

class RapidshareUpload : public Transfer
{
Q_OBJECT
public:
	RapidshareUpload();
	virtual ~RapidshareUpload();
	
	static Transfer* createInstance() { return new RapidshareUpload; }
	static int acceptable(QString url, bool);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);
	
	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return "RapidshareUpload"; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() const { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual WidgetHostChild* createOptionsWidget(QWidget*);
	
	static WidgetHostChild* createSettingsWidget(QWidget* w,QIcon&);
	static QDialog* createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers);
	
	void beginNextChunk();
protected slots:
	void queryDone(bool error);
	void postFinished(bool error);
protected:
	enum AccountType { AccountNone = 0, AccountCollector, AccountPremium };
	enum QueryType { QueryNone = 0, QueryFileInfo, QueryServerID };
	
	qint64 m_nTotal;
	QString m_strName, m_strSource, m_strMessage;
	QString m_strUsername, m_strPassword, m_strServer;
	AccountType m_type;
	QueryType m_query;
	qint64 m_nFileID, m_nKillID, m_nDone; // for resume
	bool m_bIDJustChecked;
	
	HttpEngine* m_engine;
	QHttp* m_http;
	QBuffer* m_buffer;
	
	friend class RapidshareOptsWidget;
};

class RapidshareOptsWidget : public QObject, public WidgetHostChild, Ui_RapidshareOptsForm
{
Q_OBJECT
public:
	RapidshareOptsWidget(QWidget* me, RapidshareUpload* myobj);
	RapidshareOptsWidget(QWidget* me, QList<Transfer*>* multi, QObject* parent);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
private:
	void init(QWidget* me);
	
	RapidshareUpload* m_upload;
	QList<Transfer*>* m_multi;
};

class RapidshareSettings : public QObject, public WidgetHostChild, Ui_RapidshareOptsForm
{
Q_OBJECT
public:
	RapidshareSettings(QWidget* me);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
};

#endif
