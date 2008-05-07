#include "GeneralDownloadForms.h"
#include "Settings.h"

HttpOptsWidget::HttpOptsWidget(QWidget* me,GeneralDownload* myobj) : QObject(me), m_download(myobj)
{
	setupUi(me);
	
	connect(pushUrlAdd, SIGNAL(clicked()), this, SLOT(addUrl()));
	connect(pushUrlEdit, SIGNAL(clicked()), this, SLOT(editUrl()));
	connect(pushUrlDelete, SIGNAL(clicked()), this, SLOT(deleteUrl()));
}

void HttpOptsWidget::load()
{
	lineFileName->setText(m_download->m_strFile);
	
	m_urls = m_download->m_urls;
	foreach(GeneralDownload::UrlObject obj,m_urls)
	{
		QUrl copy = obj.url;
		copy.setUserInfo(QString());
		
		listUrls->addItem(copy.toString());
	}
}

bool HttpOptsWidget::accept()
{
	return !lineFileName->text().contains('/');
}

void HttpOptsWidget::accepted()
{
	QString newFileName = lineFileName->text();
	
	if(newFileName != m_download->m_strFile)
	{
		m_download->setTargetName(newFileName);
		m_download->m_bAutoName = false;
	}
	
	m_download->m_urls = m_urls;
}

void HttpOptsWidget::addUrl()
{
	HttpUrlOptsDlg dlg((QWidget*) parent());
	
	if(dlg.exec() == QDialog::Accepted)
	{
		GeneralDownload::UrlObject obj;
		
		obj.url = dlg.m_strURL;
		obj.url.setUserName(dlg.m_strUser);
		obj.url.setPassword(dlg.m_strPassword);
		obj.strReferrer = dlg.m_strReferrer;
		obj.ftpMode = dlg.m_ftpMode;
		obj.proxy = dlg.m_proxy;
		obj.strBindAddress = dlg.m_strBindAddress;
		
		listUrls->addItem(dlg.m_strURL);
		m_urls << obj;
	}
}

void HttpOptsWidget::editUrl()
{
	int row = listUrls->currentRow();
	HttpUrlOptsDlg dlg((QWidget*) parent());
	
	if(row < 0)
		return;
	
	GeneralDownload::UrlObject& obj = m_urls[row];
	
	QUrl temp = obj.url;
	temp.setUserInfo(QString());
	dlg.m_strURL = temp.toString();
	dlg.m_strUser = obj.url.userName();
	dlg.m_strPassword = obj.url.password();
	dlg.m_strReferrer = obj.strReferrer;
	dlg.m_ftpMode = obj.ftpMode;
	dlg.m_proxy = obj.proxy;
	dlg.m_strBindAddress = obj.strBindAddress;
	
	dlg.init();
	
	if(dlg.exec() == QDialog::Accepted)
	{
		obj.url = dlg.m_strURL;
		obj.url.setUserName(dlg.m_strUser);
		obj.url.setPassword(dlg.m_strPassword);
		obj.strReferrer = dlg.m_strReferrer;
		obj.ftpMode = dlg.m_ftpMode;
		obj.proxy = dlg.m_proxy;
		obj.strBindAddress = dlg.m_strBindAddress;
		
		listUrls->item(row)->setText(dlg.m_strURL);
	}
}

void HttpOptsWidget::deleteUrl()
{
	int row = listUrls->currentRow();
	
	if(row >= 0)
	{
		delete listUrls->takeItem(row);
		m_urls.removeAt(row);
	}
}

////////////////////////////////////////////////////////////////////////////////

HttpUrlOptsDlg::HttpUrlOptsDlg(QWidget* parent, QList<Transfer*>* multi)
	: QDialog(parent), m_multi(multi)
{
	setupUi(this);
	
	if(m_multi != 0)
	{
		lineUrl->setVisible(false);
		labelUrl->setVisible(false);
	}
}

void HttpUrlOptsDlg::init()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	
	comboFtpMode->addItems(QStringList(tr("Active mode")) << tr("Passive mode"));
	comboFtpMode->setCurrentIndex(int(m_ftpMode));
	
	lineUrl->setText(m_strURL);
	lineReferrer->setText(m_strReferrer);
	lineUsername->setText(m_strUser);
	linePassword->setText(m_strPassword);
	lineAddrBind->setText(m_strBindAddress);
	
	comboProxy->addItem(tr("(none)", "No proxy"));
	comboProxy->setCurrentIndex(0);
	
	if(m_proxy.isNull() && m_multi != 0)
	{
		m_proxy = getSettingsValue("httpftp/defaultproxy").toString();
	}
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].strName);
		if(listProxy[i].uuid == m_proxy)
			comboProxy->setCurrentIndex(i+1);
	}
}

int HttpUrlOptsDlg::exec()
{
	int result;
	
	result = QDialog::exec();
	
	if(result == QDialog::Accepted)
	{
		QList<Proxy> listProxy = Proxy::loadProxys();
		
		m_strURL = lineUrl->text();
		m_strReferrer = lineReferrer->text();
		m_strUser = lineUsername->text();
		m_strPassword = linePassword->text();
		m_strBindAddress = lineAddrBind->text();
		m_ftpMode = FtpMode( comboFtpMode->currentIndex() );
		
		int ix = comboProxy->currentIndex();
		m_proxy = (!ix) ? QUuid() : listProxy[ix-1].uuid;
		
		if(m_multi != 0)
			runMultiUpdate();
	}
	
	return result;
}

void HttpUrlOptsDlg::accept()
{
	QString url = lineUrl->text();
	
	if(m_multi != 0 || url.startsWith("http://") || url.startsWith("ftp://")
#ifdef WITH_SFTP
		  || url.startsWith("sftp://")
#endif
		  || url.startsWith("https://"))
		QDialog::accept();
}

void HttpUrlOptsDlg::runMultiUpdate()
{
	// let the heuristics begin
	foreach(Transfer* t, *m_multi)
	{
		GeneralDownload::UrlObject& obj = dynamic_cast<GeneralDownload*>(t)->m_urls[0];
		if(obj.url.userInfo().isEmpty()) // we will not override the "automatic login data"
		{
			obj.url.setUserName(m_strUser);
			obj.url.setPassword(m_strPassword);
		}
		
		obj.strReferrer = m_strReferrer;
		obj.ftpMode = m_ftpMode;
		obj.proxy = m_proxy;
		obj.strBindAddress = m_strBindAddress;
	}
}

