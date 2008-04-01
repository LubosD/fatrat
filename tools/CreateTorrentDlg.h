#ifndef CREATETORRENTDLG_H
#define CREATETORRENTDLG_H
#include <QDialog>
#include <QList>
#include <QPair>
#include <QThread>
#include "ui_CreateTorrentDlg.h"
#include "libtorrent/torrent_info.hpp"

class HasherThread;
class CreateTorrentDlg : public QDialog, Ui_CreateTorrentDlg
{
Q_OBJECT
public:
	CreateTorrentDlg(QWidget* parent);
	static QWidget* create();
private:
	static void recurseDir(QList<QPair<QString, qint64> >& list, QString prefix, QString path);
public slots:
	void browseFiles();
	void browseDirs();
	void createTorrent();
	void hasherFinished();
private:
	HasherThread* m_hasher;
	QPushButton* pushCreate;
};

class HasherThread : public QThread
{
Q_OBJECT
public:
	HasherThread(QByteArray baseDir, libtorrent::torrent_info* info, QObject* parent);
	~HasherThread();
	
	virtual void run();
	const QString& error() const { return m_strError; }
	libtorrent::torrent_info* info() { return m_info; }
signals:
	void progress(int pos);
private:
	QByteArray m_baseDir;
	libtorrent::torrent_info* m_info;
	bool m_bAbort;
	QString m_strError;
};

#endif
