#ifndef GENERALDONWLOADFORMS_H
#define GENERALDONWLOADFORMS_H
#include "ui_HttpOptsWidget.h"
#include "ui_HttpUrlOptsDlg.h"
#include "WidgetHostChild.h"
#include <QDialog>
#include "GeneralDownload.h"

class HttpUrlOptsDlg : public QDialog, Ui_HttpUrlOptsDlg
{
Q_OBJECT
public:
	HttpUrlOptsDlg(QWidget* parent, QList<Transfer*>* multi = 0);
	void init();
	int exec();
	void runMultiUpdate();
	virtual void accept();

	QString m_strURL, m_strReferrer, m_strUser, m_strPassword, m_strBindAddress;
	FtpMode m_ftpMode;
	QUuid m_proxy;
	QList<Transfer*>* m_multi;
};

class HttpOptsWidget : public QObject, public WidgetHostChild, Ui_HttpOptsWidget
{
Q_OBJECT
public:
	HttpOptsWidget(QWidget* me,GeneralDownload* myobj);
	virtual void load();
	virtual void accepted();
	virtual bool accept();
public slots:
	void addUrl();
	void editUrl();
	void deleteUrl();
private:
	GeneralDownload* m_download;
	QList<GeneralDownload::UrlObject> m_urls;
};

#endif
