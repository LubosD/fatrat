#include "HelpTextBrowser.h"
#include "HelpBrowser.h"
#include <QDesktopServices>
#include <QtDebug>

HelpTextBrowser::HelpTextBrowser(QWidget* parent)
	: QTextBrowser(parent), m_engine(0)
{
}

void HelpTextBrowser::setEngine(QHelpEngine* engine)
{
	m_engine = engine;
}

QVariant HelpTextBrowser::loadResource(int type, const QUrl& name)
{
	QUrl url(name);
	
	if(name.scheme() == "http")
	{
		QMetaObject::invokeMethod(this, "setSource", Qt::QueuedConnection, Q_ARG(QUrl, m_urlLast));
		QDesktopServices::openUrl(name);
		return QVariant();
	}
	
	if(!m_engine)
		return QVariant();
	
	if(type == QTextDocument::HtmlResource)
	{
		QModelIndex index;
		QHelpContentWidget* w = m_engine->contentWidget();
		QItemSelectionModel* selmodel;
		
		m_urlLast = name;
		index = w->indexOf(m_urlLast);
		
		w->blockSignals(true);
		selmodel = w->selectionModel();
		selmodel->clearSelection();
		selmodel->select(index, QItemSelectionModel::Select);
		w->blockSignals(false);
	}
	
	if(url.isRelative())
		url = source().resolved(url);
	return m_engine->fileData(url);
}
