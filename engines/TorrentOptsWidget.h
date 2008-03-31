#ifndef TORRENTOPTSWIDGET_H
#define TORRENTOPTSWIDGET_H
#include <QObject>
#include <QTimer>
#include "ui_TorrentOptsWidget.h"
#include "WidgetHostChild.h"
#include "TorrentDownload.h"

class TorrentOptsWidget : public QObject, public WidgetHostChild, Ui_TorrentOptsWidget
{
Q_OBJECT
public:
	TorrentOptsWidget(QWidget* me, TorrentDownload* parent);
	
	virtual void load();
	virtual void accepted();
	void startInvalid();
	
	static void recursiveCheck(QTreeWidgetItem* item, Qt::CheckState state);
	static void recursiveUpdate(QTreeWidgetItem* item);
	static qint64 recursiveUpdateDown(QTreeWidgetItem* item);
public slots:
	void addUrlSeed();
	void addTracker();
	void removeTracker();
	void handleInvalid();
	
	void fileItemChanged(QTreeWidgetItem* item, int column);
private:
	TorrentDownload* m_download;
	QList<QTreeWidgetItem*> m_files;
	QStringList m_seeds;
	std::vector<libtorrent::announce_entry> m_trackers;
	bool m_bUpdating;
	QTimer m_timer;
};

#endif
