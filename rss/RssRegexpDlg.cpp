#include "fatrat.h"
#include "RssRegexpDlg.h"
#include "Queue.h"
#include <QFileDialog>
#include <QSettings>
#include <QtDebug>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

RssRegexpDlg::RssRegexpDlg(QWidget* parent)
: QDialog(parent), m_tvs(RssRegexp::None)
{
	setupUi(this);
	
	connect(lineText, SIGNAL(textChanged(const QString&)), this, SLOT(test()));
	connect(lineExpression, SIGNAL(textChanged(const QString&)), this, SLOT(test()));
	connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	
	connect(radioTVSNone, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSSeason, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSEpisode, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	connect(radioTVSDate, SIGNAL(toggled(bool)), this, SLOT(updateTVS()));
	
	m_bTVSRepacks = true;
	m_strTarget = g_settings->value("defaultdir", getSettingsDefault("defaultdir")).toString();
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

int RssRegexpDlg::exec()
{
	int r;
	
	if(m_feeds.isEmpty() || g_queues.isEmpty())
		return QDialog::Rejected;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		comboFeed->addItem(m_feeds[i].name);
		comboFeed->setItemData(i, m_feeds[i].url);
		
		if(m_feeds[i].url == m_strFeed)
			comboFeed->setCurrentIndex(i);
	}
	
	
	g_queuesLock.lockForRead();
	for(int i=0;i<g_queues.size();i++)
	{
		comboQueue->addItem(g_queues[i]->name());
		comboQueue->setItemData(i, g_queues[i]->uuid());
		if(g_queues[i]->uuid() == m_strQueue)
			comboQueue->setCurrentIndex(i);
	}
	g_queuesLock.unlock();
	
	lineExpression->setText(m_strExpression);
	lineTarget->setText(m_strTarget);
	
	switch(m_tvs)
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
	
	lineTVSFrom->setText(m_strTVSFrom);
	lineTVSTo->setText(m_strTVSTo);
	checkTVSRepacks->setChecked(m_bTVSRepacks);
	checkTVSTrailers->setChecked(m_bTVSTrailers);
	checkTVSNoManuals->setChecked(m_bTVSNoManuals);
	
	test();
	
	if((r = QDialog::exec()) == QDialog::Accepted)
	{
		m_strExpression = lineExpression->text();
		m_strTarget = lineTarget->text();
		
		m_strQueue = comboQueue->itemData(comboQueue->currentIndex()).toString();
		m_strFeed = comboFeed->itemData(comboFeed->currentIndex()).toString();
		
		m_strTVSFrom = lineTVSFrom->text();
		m_strTVSTo = lineTVSTo->text();
		
		if(radioTVSNone->isChecked())
			m_tvs = RssRegexp::None;
		else if(radioTVSSeason->isChecked())
			m_tvs = RssRegexp::SeasonBased;
		else if(radioTVSEpisode->isChecked())
			m_tvs = RssRegexp::EpisodeBased;
		else
			m_tvs = RssRegexp::DateBased;
		
		m_strFeedName = comboFeed->currentText();
		m_bTVSRepacks = checkTVSRepacks->isChecked();
		m_bTVSTrailers = checkTVSTrailers->isChecked();
		m_bTVSNoManuals = checkTVSNoManuals->isChecked();
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
