/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "fatrat.h"
#include "RssRegexpDlg.h"
#include "RssDownloadedDlg.h"
#include "Queue.h"
#include <QFileDialog>
#include <QSettings>
#include <QtDebug>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

RssRegexpDlg::RssRegexpDlg(QWidget* parent)
: QDialog(parent)
{
	setupUi(this);
	
	m_regexp.tvs = RssRegexp::None;
	
	connect(lineText, SIGNAL(textChanged(const QString&)), this, SLOT(test()));
	connect(lineExpression, SIGNAL(textChanged(const QString&)), this, SLOT(test()));
	connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	
	connect(radioTVSNone, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSSeason, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSEpisode, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSDate, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	
	connect(labelManage, SIGNAL(linkActivated(const QString&)), this, SLOT(linkClicked(const QString&)));
	
	m_regexp.includeRepacks = true;
	m_regexp.excludeManuals = true;
	m_regexp.includeTrailers = false;
	
	m_regexp.target = getSettingsValue("defaultdir").toString();
}

void RssRegexpDlg::updateTVS()
{
	const char* mask = "";
	const char *from = "", *to = "";
	
	if(radioTVSNone->isChecked())
	{
		lineTVSFrom->setEnabled(false);
		lineTVSTo->setEnabled(false);
	}
	else
	{
		lineTVSFrom->setEnabled(true);
		lineTVSTo->setEnabled(true);
		
		if(radioTVSSeason->isChecked())
		{
			mask = to = "S99E99";
			from = "S00E00";
		}
		else if(radioTVSEpisode->isChecked())
		{
			mask = to = "9999";
			from = "0000";
		}
		else if(radioTVSDate->isChecked())
		{
			mask = to = "9999-99-99";
			from = "0000-00-00";
		}
	}
	
	lineTVSFrom->setInputMask(mask);
	lineTVSTo->setInputMask(mask);
	lineTVSFrom->setText(from);
	lineTVSTo->setText(to);
}

void RssRegexpDlg::linkClicked(const QString& link)
{
	if(link != "manageDownloaded" || radioTVSNone->isChecked())
		return;
	
	const char* mask = "";
	if(radioTVSSeason->isChecked())
		mask = "S99E99";
	else if(radioTVSEpisode->isChecked())
		mask = "9999";
	else if(radioTVSDate->isChecked())
		mask = "9999-99-99";
	
	RssDownloadedDlg dlg(&m_regexp.epDone, mask, this);
	dlg.exec();
}

int RssRegexpDlg::exec()
{
	int r;
	
	if(m_feeds.isEmpty() || g_queues.isEmpty())
		return QDialog::Rejected;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		comboFeed->addItem(m_feeds[i].name);
		comboFeed->setItemData(i, m_feeds[i].url);
		
		if(m_feeds[i].url == m_regexp.source)
			comboFeed->setCurrentIndex(i);
	}
	
	
	g_queuesLock.lockForRead();
	for(int i=0;i<g_queues.size();i++)
	{
		comboQueue->addItem(g_queues[i]->name());
		comboQueue->setItemData(i, g_queues[i]->uuid());
		if(g_queues[i]->uuid() == m_regexp.queueUUID)
			comboQueue->setCurrentIndex(i);
	}
	g_queuesLock.unlock();
	
	lineExpression->setText(m_regexp.regexp.pattern());
	lineTarget->setText(m_regexp.target);
	
	switch(m_regexp.tvs)
	{
	case RssRegexp::None:
		radioTVSNone->setChecked(true); break;
	case RssRegexp::SeasonBased:
		radioTVSSeason->setChecked(true); break;
	case RssRegexp::EpisodeBased:
		radioTVSEpisode->setChecked(true); break;
	case RssRegexp::DateBased:
		radioTVSDate->setChecked(true); break;
	}
	
	lineTVSFrom->setText(m_regexp.from);
	lineTVSTo->setText(m_regexp.to);
	checkTVSRepacks->setChecked(m_regexp.includeRepacks);
	checkTVSTrailers->setChecked(m_regexp.includeTrailers);
	checkTVSNoManuals->setChecked(m_regexp.excludeManuals);
	
	test();
	
	if((r = QDialog::exec()) == QDialog::Accepted)
	{
		m_regexp.regexp = QRegExp(lineExpression->text(), Qt::CaseInsensitive);
		m_regexp.target = lineTarget->text();
		
		m_regexp.queueUUID = comboQueue->itemData(comboQueue->currentIndex()).toString();
		m_regexp.source = comboFeed->itemData(comboFeed->currentIndex()).toString();
		
		m_regexp.from = lineTVSFrom->text();
		m_regexp.to = lineTVSTo->text();
		
		if(radioTVSNone->isChecked())
			m_regexp.tvs = RssRegexp::None;
		else if(radioTVSSeason->isChecked())
			m_regexp.tvs = RssRegexp::SeasonBased;
		else if(radioTVSEpisode->isChecked())
			m_regexp.tvs = RssRegexp::EpisodeBased;
		else
			m_regexp.tvs = RssRegexp::DateBased;
		
		m_strFeedName = comboFeed->currentText();
		m_regexp.includeRepacks = checkTVSRepacks->isChecked();
		m_regexp.includeTrailers = checkTVSTrailers->isChecked();
		m_regexp.excludeManuals = checkTVSNoManuals->isChecked();
	}
	
	return r;
}

void RssRegexpDlg::browse()
{
	QString dir = QFileDialog::getExistingDirectory(this, "FatRat", lineTarget->text());
	if(!dir.isNull())
		lineTarget->setText(dir);
}

void RssRegexpDlg::test()
{
	QRegExp r(lineExpression->text(), Qt::CaseInsensitive);
	labelMatch->setPixmap( (r.indexIn(lineText->text()) != -1) ? QPixmap(":/states/completed.png") : QPixmap(":/menu/delete.png") );
}
