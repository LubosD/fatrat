#ifndef HELPTEXTBROWSER_H
#define HELPTEXTBROWSER_H
#include <QTextBrowser>

class QHelpEngine;

class HelpTextBrowser : public QTextBrowser
{
public:
	HelpTextBrowser(QWidget* parent);
	void setEngine(QHelpEngine* engine);
	QVariant loadResource(int type, const QUrl& name);
private:
	QHelpEngine* m_engine;
};

#endif
