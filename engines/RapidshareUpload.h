#ifndef RAPIDSHAREUPLOAD_H
#include "Transfer.h"
#include "HttpClient.h"

class RapidshareUpload : public Transfer
{
Q_OBJECT
public:
	RapidshareUpload();
	virtual ~RapidshareUpload();
	
	static Transfer* createInstance() { return new RapidshareUpload; }
	static int acceptable(QString url);
	
	virtual void init(QString source, QString target);
	virtual void setObject(QString source);
	
	virtual void changeActive(bool nowActive);
	virtual void setSpeedLimits(int, int up);
	
	virtual QString object() const { return m_strSource; }
	virtual QString myClass() const { return "RapidshareUpload"; }
	virtual QString name() const { return m_strName; }
	virtual QString message() const { return m_strMessage; }
	virtual Mode primaryMode() { return Upload; }
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const { return m_nTotal; }
	virtual qulonglong done() const;
	
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map);
	virtual WidgetHostChild* createOptionsWidget(QWidget*);
private:
	enum AccountType { AccountNone, AccountCollector, AccountPremium };
	qint64 m_nTotal;
	QString m_strName, m_strSource, m_strMessage;
	QString m_strUsername, m_strPassword;
	AccountType m_type;
	qint64 m_nFileID; // for resume
	
	HttpEngine* m_engine;
};

#endif
