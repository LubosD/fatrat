#ifndef TORRENTWEBVIEW_H
#define TORRENTWEBVIEW_H
#include <QWidget>
#include <QTabWidget>
#include "ui_TorrentWebView.h"

class TorrentWebView : public QWidget, public Ui_TorrentWebView
{
Q_OBJECT
public:
	TorrentWebView(QTabWidget* w);
public slots:
	void titleChanged(QString title);
	void progressChanged(int p);
	void iconChanged();
	void linkClicked(const QUrl&);
signals:
	void changeTabTitle(QString);
private:
	QTabWidget* m_tab;
};

#endif
