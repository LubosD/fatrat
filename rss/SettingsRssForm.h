#ifndef SETTINGSRSSFORM_H
#define SETTINGSRSSFORM_H
#include "ui_SettingsRssForm.h"
#include "WidgetHostChild.h"
#include <QObject>

#include "RssFetcher.h"

class SettingsRssForm : public QObject, public WidgetHostChild, Ui_SettingsRssForm
{
Q_OBJECT
public:
	SettingsRssForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
protected slots:
	void feedAdd();
	void feedEdit();
	void feedDelete();
	
	void regexpAdd();
	void regexpEdit();
	void regexpDelete();
private:
	QList<RssFeed> m_feeds;
	QList<RssRegexp> m_regexps;
};

#endif
