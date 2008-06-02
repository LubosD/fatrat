#include "TorrentWebView.h"
#include "engines/TorrentDownload.h"
#include "MainWindow.h"
#include "fatrat.h"
#include <QtDebug>

TorrentWebView::TorrentWebView(QTabWidget* w)
	: m_tab(w)
{
	setupUi(this);
	connect(browser, SIGNAL(titleChanged(const QString&)), this, SLOT(titleChanged(QString)));
	connect(browser, SIGNAL(loadProgress(int)), this, SLOT(progressChanged(int)));
	connect(browser, SIGNAL(iconChanged()), this, SLOT(iconChanged()));
	connect(browser, SIGNAL(linkClicked(const QUrl&)), this, SLOT(linkClicked(const QUrl&)));
}

void TorrentWebView::titleChanged(QString title)
{
	QString newtitle = tr("Torrent details");
	newtitle += ": ";
	
	if(title.size() > 25)
	{
		title.resize(22);
		title += "...";
	}
	newtitle += title;
	
	emit changeTabTitle(newtitle);
}

void TorrentWebView::progressChanged(int p)
{
	if(p != 100)
	{
		if(!statusbar->isVisible())
			statusbar->show();
		statusbar->setText(tr("%1% loaded").arg(p));
	}
	else
	{
		browser->page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
		statusbar->hide();
	}
}

void TorrentWebView::iconChanged()
{
	int i = m_tab->indexOf(this);
	if(i != -1)
		m_tab->setTabIcon(i, browser->icon());
	else
		qDebug() << "Warning: cannot find my own index";
}

void TorrentWebView::linkClicked(const QUrl& u)
{
	QString url = u.toString();
	qDebug() << "Clicked" << url;
	if(TorrentDownload::acceptable(url, false) >= 3)
	{
		MainWindow* wnd = (MainWindow*) getMainWindow();
		wnd->addTransfer(url, Transfer::Download, "TorrentDownload");
	}
	else
		browser->load(u);
}
