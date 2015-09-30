/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

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
