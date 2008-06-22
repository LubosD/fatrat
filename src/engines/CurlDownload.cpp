/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "CurlDownload.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "tools/HashDlg.h"
#include "CurlPoller.h"
#include <QMessageBox>
#include <QMenu>

CurlDownload::CurlDownload()
	: m_curl(0), m_nTotal(0), m_nStart(0), m_bAutoName(false), m_nUrl(0)
{
}

CurlDownload::~CurlDownload()
{
	if(m_curl)
		curl_easy_cleanup(m_curl);
}

void CurlDownload::init(QString uri, QString dest)
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
			if(!QRegExp(a.strRegExp).exactMatch(uri))
				continue;
			
			obj.url.setUserName(a.strUser);
			obj.url.setPassword(a.strPassword);
			
			enterLogMessage(tr("Loaded stored authentication data, matched regexp %1").arg(a.strRegExp));
			
			break;
		}
	}
	
	obj.proxy = getSettingsValue("httpftp/defaultproxy").toString();
	obj.ftpMode = FtpPassive;
	
	m_dir = dest;
	m_dir.mkpath(".");
	
	QString scheme = obj.url.scheme();
	if(scheme != "http" && scheme != "ftp" && scheme != "sftp" && scheme != "https")
		throw RuntimeException(tr("Unsupported protocol"));
	
	m_urls.clear();
	m_urls << obj;
	
	if(m_strFile.isEmpty())
		generateName();
}

void CurlDownload::generateName()
{
	QString name;
	if(!m_urls.isEmpty())
		name = QFileInfo(m_urls[0].url.path()).fileName();
	m_strFile = (!name.isEmpty()) ? name : "default.html";
	m_bAutoName = true;
}

int CurlDownload::acceptable(QString uri, bool)
{
	QUrl url = uri;
	QString scheme = url.scheme();
	
	if(scheme != "http" && scheme != "ftp" && scheme != "https")
		return 0;
	else
		return 1;
}

void CurlDownload::setObject(QString target)
{
	QDir dirnew = target;
	if(dirnew != m_dir)
	{
		m_dir = dirnew;
		if(isActive())
			setState(Waiting); // restart the download
	}
}

void CurlDownload::init2(QString uri,QString dest)
{
	QDir dir(dest);
	m_strFile = dir.dirName();
	dir.cdUp();
	init(uri, dir.path());
}

QString CurlDownload::object() const
{
	 return m_dir.path();
}

QString CurlDownload::name() const
{
	return m_strFile;
}

CURL* CurlDownload::curlHandle()
{
	return m_curl;
}

void CurlDownload::speedLimits(int& down, int& up)
{
	internalSpeedLimits(down, up);
}

void CurlDownload::changeActive(bool bActive)
{
	if(bActive)
	{
		if(m_urls.isEmpty())
		{
			enterLogMessage(m_strMessage = tr("No URLs have been given"));
			setState(Failed);
			return;
		}
		
		if(m_nUrl >= m_urls.size())
			m_nUrl = 0;
		
		m_file.setFileName(filePath());
		if(!m_file.open(QIODevice::WriteOnly | QIODevice::Append))
		{
			enterLogMessage(m_strMessage = m_file.errorString());
			setState(Failed);
			return;
		}
		
		QByteArray ba = m_urls[m_nUrl].url.toString().toUtf8();
		m_curl = curl_easy_init();
		
		curl_easy_setopt(m_curl, CURLOPT_URL, ba.constData());
		curl_easy_setopt(m_curl, CURLOPT_PROXY, ""); // TODO: proxy
		
		ba = m_urls[m_nUrl].strBindAddress.toUtf8();
		curl_easy_setopt(m_curl, CURLOPT_INTERFACE, ba.constData());
		
		curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, true);
		curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, true);
		curl_easy_setopt(m_curl, CURLOPT_UNRESTRICTED_AUTH, true);
		curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
		curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
		curl_easy_setopt(m_curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);
		curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, m_file.pos());
		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_function);
		curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, process_header);
		curl_easy_setopt(m_curl, CURLOPT_WRITEHEADER, this);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_debug_callback);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
		
		CurlPoller::instance()->addTransfer(this);
	}
	else
	{
		CurlPoller::instance()->removeTransfer(this);
		curl_easy_cleanup(m_curl);
		m_curl = 0;
	}
}

bool CurlDownload::writeData(const char* buffer, size_t bytes)
{
	if(!isActive())
		return false;
	
	m_file.write(buffer, bytes);
	
	return true;
}

size_t CurlDownload::process_header(const char* ptr, size_t size, size_t nmemb, CurlDownload* This)
{
	QByteArray line(ptr, size*nmemb);
	
	return size*nmemb;
}

int CurlDownload::curl_debug_callback(CURL*, curl_infotype, char* text, size_t bytes, CurlDownload* This)
{
	This->enterLogMessage(QByteArray(text, bytes));
	return 0;
}

void CurlDownload::transferDone(CURLcode result)
{
	if(!isActive())
		return;
	
	if(result == CURLE_OK)
		setState(Completed);
	else
	{
		m_strMessage = m_errorBuffer;
		setState(Failed);
	}
}

void CurlDownload::setTargetName(QString newFileName)
{
	if(m_strFile != newFileName)
	{
		m_dir.rename(m_strFile, newFileName);
		m_strFile = newFileName;
	}
}

void CurlDownload::speeds(int& down, int& up) const
{
	up = 0;
}

void CurlDownload::setSpeedLimits(int down, int)
{
}

qulonglong CurlDownload::total() const
{
	return m_nTotal;
}

qulonglong CurlDownload::done() const
{
	return QFileInfo(m_dir.filePath(name())).size();
}

void CurlDownload::load(const QDomNode& map)
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

void CurlDownload::save(QDomDocument& doc, QDomNode& map) const
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

void CurlDownload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Switch mirror"));
	connect(a, SIGNAL(triggered()), this, SLOT(switchMirror()));
	
	a = menu.addAction(tr("Compute hash..."));
	connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
}

void CurlDownload::switchMirror()
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

void CurlDownload::computeHash()
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

QString CurlDownload::filePath() const
{
	return m_dir.filePath(name());
}

/*
QDialog* CurlDownload::createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers)
{
	HttpUrlOptsDlg* obj = new HttpUrlOptsDlg(parent, &transfers);
	obj->init();
	return obj;
}

WidgetHostChild* CurlDownload::createOptionsWidget(QWidget* w)
{
	return new HttpOptsWidget(w,this);
}
*/

