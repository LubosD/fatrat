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
: QDialog(parent)
{
	setupUi(this);
	
	connect(lineText, SIGNAL(textChanged(const QString&)), this, SLOT(test()));
	connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	
	m_strTarget = g_settings->value("defaultdir", getSettingsDefault("defaultdir")).toString();
}
int RssRegexpDlg::exec()
{
	int r;
	
	if(m_feeds.isEmpty() || g_queues.isEmpty())
		return QDialog::Rejected;
	
	for(int i=0;i<m_feeds.size();i++)
	{
		comboFeed->addItem(m_feeds[i].name);
		comboQueue->setItemData(i, m_feeds[i].url);
		
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
	
	test();
	
	if((r = QDialog::exec()) == QDialog::Accepted)
	{
		m_strExpression = lineExpression->text();
		m_strTarget = lineTarget->text();
		
		m_strQueue = comboQueue->itemData(comboQueue->currentIndex()).toString();
		m_strFeed = comboFeed->itemData(comboFeed->currentIndex()).toString();
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
