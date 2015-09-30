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

#include "HelpBrowser.h"
#include <QMessageBox>
#include <QtDebug>

const char* INDEX_URL = "qthelp://info.dolezel.fatrat.10/doc/index.html";

HelpBrowser::HelpBrowser()
{
	m_engine = new QHelpEngine(DATA_LOCATION "/doc/fatrat.qhc", this);
	if(!m_engine->setupData())
	{
		QMessageBox::critical(this, tr("Help"), tr("Failed to load the documentation."));
		deleteLater();
		return;
	}
	
	setupUi(this);
	
	QHelpContentWidget* content = m_engine->contentWidget();
	splitter->insertWidget(0, content);
	splitter->setSizes(QList<int>() << 80 << 400);
	content->setRootIsDecorated(false);
	
	textBrowser->setEngine(m_engine);
	
	connect(content, SIGNAL(linkActivated(const QUrl&)), this, SLOT(openPage(const QUrl&)));
	connect(content, SIGNAL(clicked(const QModelIndex&)), this, SLOT(itemClicked(const QModelIndex&)));
	connect(textBrowser, SIGNAL(sourceChanged(const QUrl&)), this, SLOT(sourceChanged()));
	
	openPage( QUrl(INDEX_URL) );
	
	QTimer::singleShot(100, content, SLOT(expandAll()));
	QTimer::singleShot(100, this, SLOT(sourceChanged()));
}

void HelpBrowser::openPage(const QUrl& url)
{
	textBrowser->setSource(url);
}

void HelpBrowser::sourceChanged()
{
	emit changeTabTitle( tr("Help") + ": " + textBrowser->documentTitle() );
}

void HelpBrowser::itemClicked(const QModelIndex& index)
{
	openPage( m_engine->contentModel()->contentItemAt(index)->url() );
}
