#include "fatrat.h"
#include "GeneralDownload.h"
#include "HttpFtpSettings.h"
#include "tools/HashDlg.h"
#include "RuntimeException.h"

#include "HttpClient.h"
#include "FtpClient.h"

#ifdef WITH_SFTP
#	include "SftpClient.h"
#endif

#include <QtDebug>
#include <QThread>
#include <QTcpSocket>
#include <QSettings>
#include <QMessageBox>
#include <QMenu>

using namespace std;

extern QSettings* g_settings;

GeneralDownload::GeneralDownload(bool local) : Transfer(local), m_nTotal(0), m_nStart(0),
m_bAutoName(false), m_engine(0), m_nUrl(0)
{
}

GeneralDownload::~GeneralDownload()
{
	if(m_engine)
		m_engine->destroy();
}

int GeneralDownload::acceptable(QString uri, bool)
{
	QUrl url = uri;
	QString scheme = url.scheme();
	
	if(scheme != "http" && scheme != "ftp"
#ifdef WITH_SFTP
		&& scheme != "sftp"
#endif
		&& scheme != "https")
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
	
	int hash = uri.lastIndexOf("#__filename=");
	if(hash != -1)
	{
		QString name = QUrl::fromPercentEncoding(uri.mid(hash+12).toUtf8());
		m_strFile = name;
		uri.resize(hash);
	}
	hash = uri.indexOf('#');
	if(hash != -1)
		uri.resize(hash);
	
	obj.url = uri;
	
	if(obj.url.userInfo().isEmpty())
	{
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
	}
	
	obj.proxy = g_settings->value("httpftp/defaultproxy").toString();
	obj.ftpMode = FtpPassive;
	
	m_dir = dest;
	
	QString scheme = obj.url.scheme();
	if(scheme != "http" && scheme != "ftp" && scheme != "sftp" && scheme != "https")
		throw RuntimeException(tr("Unsupported protocol"));
	
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
	m_bAutoName = true;
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
		if(m_engine)
			down = m_engine->speed();
	}
}

void GeneralDownload::load(const QDomNode& map)
{
	m_dir = getXMLProperty(map, "dir");
	m_nTotal = getXMLProperty(map, "knowntotal").toULongLong();
	m_strFile = getXMLProperty(map, "filename");
	m_bAutoName = getXMLProperty(map, "autoname").toInt() != 0;
	
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

void GeneralDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "dir", m_dir.path());
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
	setXMLProperty(doc, map, "filename", m_strFile);
	setXMLProperty(doc, map, "autoname", QString::number(m_bAutoName));
	
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
			
			if(scheme == "http" || scheme == "https" || (Proxy::getProxy(m_urls[m_nUrl].proxy).nType == Proxy::ProxyHttp && scheme == "ftp"))
				startHttp(m_urls[m_nUrl].url,m_urls[m_nUrl].strReferrer);
			else if(scheme == "ftp")
				startFtp(m_urls[m_nUrl].url);
#ifdef WITH_SFTP
			else if(scheme == "sftp")
				startSftp(m_urls[m_nUrl].url);
#endif
			else
			{
				m_strMessage = tr("Unsupported protocol");
				setState(Failed);
				return;
			}
			
			int down, up;
			internalSpeedLimits(down, up);
			m_engine->setLimit(down);
		}
	}
	else
	{
		if(m_engine)
		{
			m_engine->destroy();
			m_engine = 0;
		}
	}
}

void GeneralDownload::connectSignals()
{
	connect(m_engine, SIGNAL(receivedSize(qint64)), this, SLOT(responseSizeReceived(qint64)), Qt::DirectConnection);
	connect(m_engine, SIGNAL(finished(bool)), this, SLOT(requestFinished(bool)), Qt::QueuedConnection);
	connect(m_engine, SIGNAL(statusMessage(QString)), this, SLOT(changeMessage(QString)));
	connect(m_engine, SIGNAL(logMessage(QString)), this, SLOT(enterLogMessage(QString)));
}

void GeneralDownload::startHttp(QUrl url, QUrl referrer)
{
	qDebug() << "GeneralDownload::startHttp" << url;
	
	m_urlLast = url;
	m_engine = new HttpEngine(url, referrer, m_urls[m_nUrl].proxy);
	
	connect(m_engine, SIGNAL(redirected(QString)), this, SLOT(redirected(QString)));
	connect(m_engine, SIGNAL(renamed(QString)), this, SLOT(renamed(QString)));
	connectSignals();
	
	m_engine->bind(QHostAddress(m_urls[m_nUrl].strBindAddress));
	m_engine->request(filePath(), false, 0);
}

void GeneralDownload::startFtp(QUrl url)
{
	qDebug() << "GeneralDownload::startFtp" << url;
	
	m_urlLast = url;
	m_engine = new FtpEngine(url, m_urls[m_nUrl].proxy);
	
	connectSignals();
	
	m_engine->request(filePath(), false, (m_urls[m_nUrl].ftpMode == FtpActive ? FtpEngine::FtpActive : FtpEngine::FtpPassive));
}

void GeneralDownload::startSftp(QUrl url)
{
#ifdef WITH_SFTP
	qDebug() << "GeneralDownload::startSftp" << url;
	
	m_urlLast = url;
	m_engine = new SftpEngine(url);
	
	connectSignals();
	
	//m_engine->bind(QHostAddress(m_urls[m_nUrl].strBindAddress));
	m_engine->request(filePath(), false, 0);
#else
	qDebug() << "GeneralDownload::startSftp() should have never been called!";
	abort();
#endif
}

void GeneralDownload::responseSizeReceived(qint64 totalsize)
{
	m_nTotal = totalsize;
}

void GeneralDownload::requestFinished(bool error)
{
	void* obj = sender();
	
	if(isActive() && m_engine == obj)
	{
		m_strMessage = ((LimitedSocket*) obj)->errorString();
		if(error)
		{
			enterLogMessage(tr("Transfer has failed: %1").arg(m_strMessage));
			setState(Failed);
		}
		else if(isActive())
		{
			if(m_engine)
			{
				m_engine->destroy();
				m_engine = 0;
			}
			
			enterLogMessage(tr("Transfer has been completed"));
			setState(Completed);
		}
	}
}

void GeneralDownload::redirected(QString newurl)
{
	if(!newurl.isEmpty())
	{
		QUrl location = newurl;
		QString scheme = location.scheme();
		
		location.setUserInfo(m_urlLast.userInfo());
		m_urlLast.setUserInfo(QString());
		
		qDebug() << "Redirected to: " << m_urlLast << endl;
		enterLogMessage(tr("We're being redirected to: %1").arg(m_urlLast.toString()));
		
		if(scheme == "http" || scheme == "ftp" || scheme == "https")
		{
			if(m_bAutoName)
			{
				QString newname = QFileInfo(newurl).fileName();
				if(!newname.isEmpty())
					setTargetName(newname);
			}
			
			if(scheme == "http" || scheme == "https")
				startHttp(location, m_urlLast);
			else
				startFtp(location);
			
			m_strMessage = tr("Redirected");
		
			//if(code == 301)
			//	m_urls[m_nUrl].url = location;
			int down, up;
			internalSpeedLimits(down, up);
			m_engine->setLimit(down);
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

void GeneralDownload::setSpeedLimits(int down,int)
{
	if(down < 1000)
		down = 0;
	if(m_engine)
		m_engine->setLimit(down);
}

void GeneralDownload::renamed(QString dispName)
{
	if(m_bAutoName)
	{
		dispName.replace('/', '_');
		qDebug() << "Automatically renaming to" << dispName;
		setTargetName(dispName);
	}
}

void GeneralDownload::setTargetName(QString newFileName)
{
	if(m_strFile != newFileName)
	{
		m_dir.rename(m_strFile, newFileName);
		m_strFile = newFileName;
	}
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
	QAction* a;
	
	a = menu.addAction(tr("Switch mirror"));
	connect(a, SIGNAL(triggered()), this, SLOT(switchMirror()));
	
	a = menu.addAction(tr("Compute hash..."));
	connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
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

void GeneralDownload::computeHash()
{
	if(state() != Completed)
	{
		if(QMessageBox::warning(getMainWindow(), "FatRat", tr("You're about to compute hash from an incomplete download."),
		   QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
			return;
	}
	
	HashDlg dlg(getMainWindow(), filePath());
	dlg.exec();
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

