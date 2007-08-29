#ifndef _TORRENTDOWNLOAD_H
#define _TORRENTDOWNLOAD_H
#include "Transfer.h"
#include "torrent/torrentclient.h"
#include "torrent/metainfo.h"
#include "WidgetHostChild.h"
#include "ui_SettingsTorrentForm.h"
#include "ui_TorrentOptsWidget.h"
#include <QTimer>

class TorrentDownload : public Transfer
{
Q_OBJECT
public:
	TorrentDownload();
	virtual ~TorrentDownload();
	
	virtual void changeActive(bool nowActive);
	virtual QString name() const;
	virtual void speeds(int& down, int& up) const;
	virtual qulonglong total() const;
	virtual qulonglong done() const;
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map);
	virtual bool supportsDistribute() { return true; }
	virtual QString myClass() const { return "TorrentDownload"; }
	virtual QString message() const { return m_strMessage; }
	
	virtual void init(QString uri,QString target);
	virtual void setObject(QString object);
	virtual QString object() const;
	virtual void setSpeedLimits(int down,int up);
	virtual WidgetHostChild* createOptionsWidget(QWidget* w);
	
	static int acceptable(QString uri);
	static Transfer* createInstance() { return new TorrentDownload; }
	static WidgetHostChild* createSettingsWidget(QWidget* w,QIcon&);
	static void globalInit();
protected:
	void initFileDownload(QString uri,QString target);
public slots:
	void init();
	void updateMessage();
	void uploadRateUpdated(int v) { m_nUp=v; }
	void downloadRateUpdated(int v) { m_nDown=v; }
	void clientStateChanged(TorrentClient::State state);
	void checkRatio();
	
	// .torrent download
	void stateChanged(Transfer::State prev, Transfer::State now);
protected:
	TorrentClient* m_pClient;
	MetaInfo m_metaInfo;
	int m_nDown, m_nUp;
	QString m_strTarget,m_strMessage;
	QTimer m_timer;
	QMap<QString,bool> m_mapWanted;
	
	friend class TorrentOptsWidget;
};

class TorrentOptsWidget : public QObject, public WidgetHostChild, Ui_TorrentOptsWidget
{
Q_OBJECT
public:
	TorrentOptsWidget(QWidget* me,TorrentDownload* myobj);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
public slots:
	void updateStatus();
private:
	struct Directory;
	Directory& getDirectory(QString path);
	void fillTree();
	static void generateMap(QMap<QString,bool>& map, Directory& dir, QString prefix);
private:
	TorrentDownload* m_download;
	
	struct File
	{
		QString name;
		bool download;
		QTreeWidgetItem* item;
	};
	struct Directory
	{
		QString name;
		QList<Directory> dirs;
		QList<File> files;
		QTreeWidgetItem* item;
	} m_toplevel;
};

class TorrentDownloadSettings : public WidgetHostChild, Ui_SettingsTorrentForm
{
public:
	TorrentDownloadSettings(QWidget* w) { setupUi(w); }
	virtual void load();
	virtual bool accept();
	virtual void accepted();
};

#endif
