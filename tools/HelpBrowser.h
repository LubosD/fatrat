#ifndef HELPBROWSER_H
#define HELPBROWSER_H
#include "config.h"
#include "ui_HelpBrowser.h"
#include <QWidget>
#include <QtGlobal>

#ifndef WITH_DOCUMENTATION
#	error This file is not supposed to be included!
#endif

#if QT_VERSION < 0x040400
#	error You need at least Qt 4.4 for this feature
#endif

#include <QtHelp/QtHelp>

class HelpBrowser : public QWidget, Ui_HelpBrowser
{
Q_OBJECT
public:
	HelpBrowser();
public slots:
	void openPage(const QUrl& url);
	void itemClicked(const QModelIndex& index);
private:
	QHelpEngine* m_engine;
};

#endif
