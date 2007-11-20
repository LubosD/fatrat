#ifndef _FAKEDOWNLOAD_H
#define _FAKEDOWNLOAD_H

class FakeDownload : public Transfer
{
public:
	virtual void changeActive(bool) {}
	virtual void speeds(int& down, int& up) const { down=up=1024; }
	virtual qulonglong total() const { return 1024*1024; }
	virtual qulonglong done() const { return 1024*512; }
	virtual QString name() const { return m_strName; }
	virtual QString myClass() const { return "FakeDownload"; }
	
	virtual void init(QString uri,QString)
	{
		m_strName = uri;
	}
	
	virtual void load(const QDomNode& map)
	{
		Transfer::load(map);
		m_strName = getXMLProperty(map, "name");
	}
	
	virtual void save(QDomDocument& doc, QDomNode& map)
	{
		Transfer::save(doc, map);
		//map["name"] = m_strName;
		setXMLProperty(doc, map, "name", m_strName);
	}
	
	void setSpeedLimits(int,int) {}
	virtual QString uri() const { return m_strName; }
	virtual QString object() const { return QString(); }
	virtual void setObject(QString) { }
	
	static Transfer* createInstance() { return new FakeDownload; }
	static int acceptable(QString, bool) { return 1; }
private:
	QString m_strName;
};

#endif
