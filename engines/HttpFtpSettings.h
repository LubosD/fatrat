#ifndef HTTPFTPSETTINGS_H
#define HTTPFTPSETTINGS_H
#include "ui_SettingsHttpForm.h"
#include "WidgetHostChild.h"
#include <QObject>
#include "fatrat.h"

class HttpFtpSettings : public QObject, public WidgetHostChild, Ui_SettingsHttpForm
{
Q_OBJECT
public:
	HttpFtpSettings(QWidget* w);
	virtual void load();
	virtual void accepted();
public slots:
	void authAdd();
	void authEdit();
	void authDelete();
private:
	QList<Proxy> m_listProxy;
	QUuid m_defaultProxy;
	
	QList<Auth> m_listAuth;
};

#endif
