#include "HelpTextBrowser.h"
#include "HelpBrowser.h"
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
	if(m_engine)
	{
		QUrl url(name);
		if(url.isRelative())
			url = source().resolved(url);
		return m_engine->fileData(url);
	}
	return QVariant();
}
