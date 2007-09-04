#include "fatrat.h"
#include "GeneralDownload.h"
#include "HttpFtpSettings.h"

#include <iostream>
#include <QtDebug>
#include <QThread>
#include <QTcpSocket>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>
#include <stdexcept>

using namespace std;

extern QSettings* g_settings;

GeneralDownload::GeneralDownload(bool local) : Transfer(local), m_nTotal(0), m_nStart(0), m_bSupportsResume(false),
m_http(0), m_ftp(0), m_nUrl(0)
{
}

GeneralDownload::~GeneralDownload()
{
	qDebug() << "Destroying generalDownload\n";
	if(m_http)
		m_http->destroy();
	if(m_ftp)
		m_ftp->destroy();
	qDebug() << "GeneralDownload::~GeneralDownload() exiting\n";
}

int GeneralDownload::acceptable(QString uri)
{
	QUrl url = uri;
	QString scheme = url.scheme();
	
	if(scheme != "http" && scheme != "ftp")
		return 0;
	else
		return 2;
}

QString GeneralDownload::name() const
{
	return m_strFile;
}

void GeneralDownload::init(QString uri,QString dest)
{
	UrlObject obj;
	
	obj.url = uri;
	QList<Auth> auths = Auth::loadAuths();
	foreach(Auth a,auths)
	{
		if(QRegExp(a.strRegExp).exactMatch(uri))
		{
			obj.url.setUserName(a.strUser);
			obj.url.setPassword(a.strPassword);
			
			enterLogMessage(tr("Loaded stored authentication data, matched regexp %1").arg(a.strRegExp));
			
			break;
		}
	}
	
	obj.proxy = g_settings->value("httpftp/defaultproxy").toString();
	obj.ftpMode = FtpPassive;
	
	m_dir = dest;
	
	if(obj.url.scheme() != "http" && obj.url.scheme() != "ftp")
		throw std::runtime_error("Unsupported protocol");
	
	m_urls.clear();
	m_urls << obj;
	
	if(m_strFile.isEmpty())
		generateName();
}

void GeneralDownload::generateName()
{
	QString name;
	if(!m_urls.isEmpty())
		name = QFileInfo(m_urls[0].url.path()).fileName();
	m_strFile = (!name.isEmpty()) ? name : "default.html";
}

void GeneralDownload::setObject(QString target)
{
	QDir dirnew = target;
	if(dirnew != m_dir)
	{
		m_dir = dirnew;
		if(isActive())
			setState(Waiting); // restart download
	}
}

void GeneralDownload::init2(QString uri,QString dest)
{
	QDir dir(dest);
	m_strFile = dir.dirName();
	dir.cdUp();
	init(uri, dir.path());
}

void GeneralDownload::speeds(int& down, int& up) const
{
	up = down = 0;
	if(isActive())
	{
		if(m_http)
			down = m_http->speed();
		if(m_ftp)
			down = m_ftp->speed();
	}
}

void GeneralDownload::load(const QDomNode& map)
{
	m_dir = getXMLProperty(map, "dir");
	m_nTotal = getXMLProperty(map, "knowntotal").toULongLong();
	m_bSupportsResume = getXMLProperty(map, "supportsresume").toInt() != 0;
	
	m_strFile = getXMLProperty(map, "filename");
	
	QDomElement url = map.firstChildElement("url");
	while(!url.isNull())
	{
		UrlObject obj;
		
		obj.url = getXMLProperty(url, "address");
		obj.strReferrer = getXMLProperty(url, "referrer");
		obj.proxy = getXMLProperty(url, "proxy");
		obj.ftpMode = (FtpMode) getXMLProperty(url, "ftpmode").toInt();
		obj.strBindAddress = getXMLProperty(url, "bindip");
		
		url = url.nextSiblingElement("url");
		
		m_urls << obj;
	}
	
	if(m_strFile.isEmpty())
		generateName();
	
	Transfer::load(map);
}

void GeneralDownload::save(QDomDocument& doc, QDomNode& map)
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "dir", m_dir.path());
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
	setXMLProperty(doc, map, "supportsresume", QString::number(int(m_bSupportsResume)));
	setXMLProperty(doc, map, "filename", m_strFile);
	
	for(int i=0;i<m_urls.size();i++)
	{
		QDomElement sub = doc.createElement("url");
		//sub.setAttribute("id", QString::number(i));
		
		setXMLProperty(doc, sub, "address", m_urls[i].url.toString());
		setXMLProperty(doc, sub, "referrer", m_urls[i].strReferrer);
		setXMLProperty(doc, sub, "proxy", m_urls[i].proxy.toString());
		setXMLProperty(doc, sub, "ftpmode", QString::number( (int) m_urls[i].ftpMode ));
		setXMLProperty(doc, sub, "bindip", m_urls[i].strBindAddress);
		
		map.appendChild(sub);
	}
}

qulonglong GeneralDownload::done() const
{
	return QFileInfo(m_dir.filePath(name())).size();
}

void GeneralDownload::changeActive(bool nowActive)
{
	if(nowActive)
	{
		if(m_urls.isEmpty())
		{
			m_strMessage = tr("No URLs have been given");
			enterLogMessage(tr("No URLs have been given"));
		}
		else
		{
			if(m_nUrl >= m_urls.size())
				m_nUrl = 0;
			
			enterLogMessage(tr("Downloading URL %1").arg(m_urls[m_nUrl].url.toString()));
			
			m_strMessage.clear();
			m_urlLast = QUrl();
			
			QString scheme = m_urls[m_nUrl].url.scheme();
			
			if(scheme == "http" || (Proxy::getProxyType(m_urls[m_nUrl].proxy) == Proxy::ProxyHttp && scheme == "ftp"))
				startHttp(m_urls[m_nUrl].url,m_urls[m_nUrl].strReferrer);
			else if(scheme == "ftp")
				startFtp(m_urls[m_nUrl].url);
		}
	}
	else
	{
		if(m_http)
		{
			m_http->destroy();
			m_http = 0;
		}
		if(m_ftp)
		{
			m_ftp->destroy();
			m_ftp = 0;
		}
	}
}

void GeneralDownload::startHttp(QUrl url, QUrl referrer)
{
	qDebug() << "GeneralDownload::startHttp" << url;
	
	m_urlLast = url;
	
	m_http = new HttpEngine(url, referrer, m_urls[m_nUrl].proxy);
	
	connect(m_http, SIGNAL(responseReceived(QHttpResponseHeader)), this, SLOT(responseHeaderReceived(QHttpResponseHeader)), Qt::DirectConnection);
	connect(m_http, SIGNAL(finished(void*,bool)), this, SLOT(requestFinished(void*,bool)), Qt::QueuedConnection);
	
	m_http->bind(QHostAddress(m_urls[m_nUrl].strBindAddress));
	m_http->request(filePath());
}

void GeneralDownload::startFtp(QUrl url)
{
	qDebug() << "GeneralDownload::startFtp" << url;
	
	m_ftp = new FtpEngine(url, m_urls[m_nUrl].proxy);
	
	connect(m_ftp, SIGNAL(finished(void*,bool)), this, SLOT(requestFinished(void*,bool)), Qt::QueuedConnection);
	connect(m_ftp, SIGNAL(responseReceived(qulonglong)), this, SLOT(responseSizeReceived(qulonglong)), Qt::QueuedConnection);
	connect(m_ftp, SIGNAL(status(QString)), this, SLOT(changeMessage(QString)));
	connect(m_ftp, SIGNAL(logMessage(QString)), this, SLOT(enterLogMessage(QString)));
	
	m_ftp->request(filePath(), FtpEngine::FtpGet | (m_urls[m_nUrl].ftpMode == FtpActive ? FtpEngine::FtpActive : FtpEngine::FtpPassive));
}

void GeneralDownload::responseSizeReceived(qulonglong totalsize)
{
	m_nTotal = totalsize;
}

void GeneralDownload::requestFinished(void* obj, bool error)
{
	cout << "GeneralDownload::requestFinished()\n";
	
	if(isActive() && (m_http == obj || m_ftp == obj))
	{
		if(error)
		{
			m_strMessage = ((LimitedSocket*) obj)->errorString();
			enterLogMessage(tr("Transfer has failed: %1").arg(m_strMessage));
			setState(Failed);
		}
		else if(isActive())
		{
			m_strMessage.clear();
			enterLogMessage(tr("Transfer has been completed"));
			setState(Completed);
			
			if(m_http)
			{
				m_http->destroy();
				m_http = 0;
			}
			if(m_ftp)
			{
				m_ftp->destroy();
				m_ftp = 0;
			}
		}
	}
}

void GeneralDownload::responseHeaderReceived(QHttpResponseHeader resp)
{
	int code = resp.statusCode();
	
	enterLogMessage(tr("Received HTTP response code %1").arg(code));
	
	if(!isActive())
		return;
	
	if(code == 302 || code == 301) // HTTP 302 Found, 301 Moved Permanently
	{
		// we're being redirected to another URL
		if(resp.hasKey("location"))
		{
			QUrl location = resp.value("location");
			QString scheme = location.scheme();
			
			location.setUserInfo(m_urlLast.userInfo());
			m_urlLast.setUserInfo(QString());
			
			qDebug() << "Redirected to: " << location << endl;
			enterLogMessage(tr("We're being redirected to: %1").arg(location.toString()));
			
			if(scheme == "http" || scheme == "ftp")
			{
				if(scheme == "http")
					startHttp(location, m_urlLast);
				else
					startFtp(location);
				
				m_strMessage = tr("Redirected");
			
				if(code == 301)
					m_urls[m_nUrl].url = location;
			}
			else
			{
				m_strMessage = tr("Invalid redirect");
				enterLogMessage(tr("We've been redirected to an unsupported URL: %1").arg(location.toString()));
				setState(Failed);
			}
		}
		else
		{
			m_strMessage = tr("Invalid redirect");
			enterLogMessage(tr("We've been redirected, but no new URL has been given"));
			setState(Failed);
		}
	}
	else if(code == 200) // HTTP 200 OK
	{
		m_bSupportsResume = false;
		
		if(resp.hasKey("content-length"))
			m_nTotal = resp.value("content-length").toULongLong();
		else
			m_nTotal = 0;
		
		if(done())
		{
			m_strMessage = tr("Restarting download");
			enterLogMessage(tr("Server does not support resume, transfer has to be restarted"));
			
//			m_file.resize(0);
//			m_file.seek(0);
		}
		else
			m_strMessage = tr("Probably no resume support");
	}
	else if(code == 206) // HTTP 206 Partial Content
	{
		m_bSupportsResume = true;
		
		if(resp.hasKey("content-length"))
			m_nTotal = done() + resp.value("content-length").toULongLong();
		else
			m_nTotal = 0;
		
		//m_strMessage = "Downloading";
	}
	else
	{
		m_strMessage = resp.reasonPhrase();
		setState(Failed);
	}
}

void GeneralDownload::setSpeedLimits(int down,int)
{
	if(down)
		qDebug() << "Down speed limit:" << down;
	if(down < 1000)
		down = 0;
	if(m_http)
		m_http->setLimit(down);
	if(m_ftp)
		m_ftp->setLimit(down);
}

WidgetHostChild* GeneralDownload::createOptionsWidget(QWidget* w)
{
	return new HttpOptsWidget(w,this);
}

WidgetHostChild* GeneralDownload::createSettingsWidget(QWidget* w,QIcon& i)
{
	i = QIcon(":/fatrat/httpftp.png");
	return new HttpFtpSettings(w);
}

void GeneralDownload::fillContextMenu(QMenu& menu)
{
	QAction* a = menu.addAction(tr("Switch mirror"));
	connect(a, SIGNAL(triggered()), this, SLOT(switchMirror()));
}

void GeneralDownload::switchMirror()
{
	int prev,cur;
	prev = cur = m_nUrl;
	
	cur++;
	if(cur >= m_urls.size())
		cur = 0;
	
	if(cur == prev)
		enterLogMessage(tr("No mirror to switch to!"));
	else
	{
		enterLogMessage(tr("Switching mirror: %1 -> %2").arg(m_urls[prev].url.toString()).arg(m_urls[cur].url.toString()));
		m_nUrl = cur;
		
		if(isActive())
		{
			changeActive(false);
			changeActive(true);
		}
	}
}

QDialog* GeneralDownload::createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers)
{
	HttpUrlOptsDlg* obj = new HttpUrlOptsDlg(parent, &transfers);
	obj->init();
	return obj;
}

/////////////////////////////////////////////////////////////////////////////////

HttpOptsWidget::HttpOptsWidget(QWidget* me,GeneralDownload* myobj) : QObject(me), m_download(myobj)
{
	setupUi(me);
	
	connect(pushUrlAdd, SIGNAL(clicked()), this, SLOT(addUrl()));
	connect(pushUrlEdit, SIGNAL(clicked()), this, SLOT(editUrl()));
	connect(pushUrlDelete, SIGNAL(clicked()), this, SLOT(deleteUrl()));
}

void HttpOptsWidget::load()
{
	cout << "HttpOptsWidget::load()\n";
	
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
		m_download->m_dir.rename(m_download->m_strFile, newFileName);
		m_download->m_strFile = newFileName;
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
		m_proxy = g_settings->value("httpftp/defaultproxy").toString();
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
	
	if(m_multi != 0 || url.startsWith("http://") || url.startsWith("ftp://"))
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

