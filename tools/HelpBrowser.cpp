#include "HelpBrowser.h"
#include <QMessageBox>
#include <QtDebug>

const char* INDEX_URL = "qthelp://info.dolezel.fatrat.1_0/doc/index.html";

HelpBrowser::HelpBrowser()
{
	m_engine = new QHelpEngine(DATA_LOCATION "/doc/fatrat.qhc", this);
	if(!m_engine->setupData())
	{
		QMessageBox::critical(this, tr("Help"), tr("Failed to load the documentation."));
		return;
	}
	
	setupUi(this);
	
	QHelpContentWidget* content = m_engine->contentWidget();
	splitter->insertWidget(0, content);
	splitter->setSizes(QList<int>() << 80 << 400);
	
	textBrowser->setEngine(m_engine);
	
	connect(content, SIGNAL(linkActivated(const QUrl&)), this, SLOT(openPage(const QUrl&)));
	connect(content, SIGNAL(clicked(const QModelIndex&)), this, SLOT(itemClicked(const QModelIndex&)));
	
	openPage( QUrl(INDEX_URL) );
}

void HelpBrowser::openPage(const QUrl& url)
{
	textBrowser->setSource(url);
}

void HelpBrowser::itemClicked(const QModelIndex& index)
{
	openPage( m_engine->contentModel()->contentItemAt(index)->url() );
}
