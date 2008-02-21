#include "SettingsRssForm.h"
#include "RssFeedDlg.h"
#include "RssRegexpDlg.h"

SettingsRssForm::SettingsRssForm(QWidget* w, QObject* parent)
	: QObject(parent)
{
	setupUi(w);
	
	connect(pushFeedAdd, SIGNAL(clicked()), this, SLOT(feedAdd()));
	connect(pushFeedEdit, SIGNAL(clicked()), this, SLOT(feedEdit()));
	connect(pushFeedDelete, SIGNAL(clicked()), this, SLOT(feedDelete()));
	
	connect(pushRegexpAdd, SIGNAL(clicked()), this, SLOT(regexpAdd()));
	connect(pushRegexpEdit, SIGNAL(clicked()), this, SLOT(regexpEdit()));
	connect(pushRegexpDelete, SIGNAL(clicked()), this, SLOT(regexpDelete()));
}

void SettingsRssForm::load()
{
	m_feeds.clear();
	m_regexps.clear();
	
	listFeeds->clear();
	listRegexps->clear();
	
	RssFetcher::loadRegexps(m_regexps);
	RssFetcher::loadFeeds(m_feeds);
	
	foreach(RssFeed feed, m_feeds)
		listFeeds->addItem(feed.name);
	
	foreach(RssRegexp regexp, m_regexps)
		listRegexps->addItem(regexp.regexp.pattern());
}

void SettingsRssForm::accepted()
{
	RssFetcher::saveRegexps(m_regexps);
	RssFetcher::saveFeeds(m_feeds);
}

void SettingsRssForm::feedAdd()
{
	RssFeedDlg dlg(listFeeds->parentWidget());
	if(dlg.exec() == QDialog::Accepted)
	{
		RssFeed feed;
		feed.name = dlg.m_strName;
		feed.url = dlg.m_strURL;
		m_feeds << feed;
		
		listFeeds->addItem(feed.name);
	}
}

void SettingsRssForm::feedEdit()
{
	int ix;
	
	if((ix = listFeeds->currentRow()) < 0)
		return;
	
	RssFeedDlg dlg(listFeeds->parentWidget());
	
	dlg.m_strName = m_feeds[ix].name;
	dlg.m_strURL = m_feeds[ix].url;
	
	if(dlg.exec() == QDialog::Accepted)
	{
		if(m_feeds[ix].url != dlg.m_strURL)
		{
			for(int i=0;i<m_regexps.size();i++)
			{
				if(m_regexps[i].source == m_feeds[ix].url)
					m_regexps[i].source = dlg.m_strURL;
			}
		}
		
		m_feeds[ix].name = dlg.m_strName;
		m_feeds[ix].url = dlg.m_strURL;
		
		listFeeds->item(ix)->setText(m_feeds[ix].name);
	}
}

void SettingsRssForm::feedDelete()
{
	int ix;
	
	if((ix = listFeeds->currentRow()) < 0)
		return;
	
	for(int i=0;i<m_regexps.size();i++)
	{
		if(m_regexps[i].source == m_feeds[ix].url)
			m_regexps.removeAt(i--);
	}
	
	m_feeds.removeAt(ix);
	
	delete listFeeds->takeItem(ix);
}


void SettingsRssForm::regexpAdd()
{
	RssRegexpDlg dlg(listFeeds->parentWidget());
	dlg.m_feeds = m_feeds;
	
	if(dlg.exec() == QDialog::Accepted)
	{
		RssRegexp r;
		r.source = dlg.m_strFeed;
		r.target = dlg.m_strTarget;
		r.regexp = QRegExp(dlg.m_strExpression);
		r.queueUUID = dlg.m_strQueue;
		
		listRegexps->addItem(dlg.m_strExpression);
		
		m_regexps << r;
	}
}

void SettingsRssForm::regexpEdit()
{
	int ix;
	
	if((ix = listRegexps->currentRow()) < 0)
		return;
	
	RssRegexpDlg dlg(listFeeds->parentWidget());
	dlg.m_feeds = m_feeds;
	dlg.m_strFeed = m_regexps[ix].source;
	dlg.m_strTarget = m_regexps[ix].target;
	dlg.m_strExpression = m_regexps[ix].regexp.pattern();
	dlg.m_strQueue = m_regexps[ix].queueUUID;
	
	if(dlg.exec() == QDialog::Accepted)
	{
		m_regexps[ix].source = dlg.m_strFeed;
		m_regexps[ix].target = dlg.m_strTarget;
		m_regexps[ix].regexp = QRegExp(dlg.m_strExpression, Qt::CaseInsensitive);
		m_regexps[ix].queueUUID = dlg.m_strQueue;
		
		listRegexps->item(ix)->setText(dlg.m_strExpression);
	}
}

void SettingsRssForm::regexpDelete()
{
	int ix;
	
	if((ix = listRegexps->currentRow()) < 0)
		return;
	m_regexps.removeAt(ix);
	
	delete listRegexps->takeItem(ix);
}
