/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/


#include "config.h"
#include "CurlDownload.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "GeneralDownloadForms.h"
#include "HttpFtpSettings.h"
#include "tools/HashDlg.h"
#include "CurlPoller.h"
#include "Auth.h"
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <QMessageBox>
#include <QMenu>
#include <QtDebug>
#include <iostream>
#include <errno.h>

CurlDownload::CurlDownload()
	: m_curl(0), m_nTotal(0), m_nStart(0), m_bAutoName(false), m_nUrl(0), m_file(0)
{
	m_errorBuffer[0] = 0;
}

CurlDownload::~CurlDownload()
{
	if(isActive())
		changeActive(false);
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
	
	obj.url = QUrl::fromPercentEncoding(uri.toUtf8());
	
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
		throw RuntimeException(tr("Unsupported protocol: \"%1\"").arg(scheme));
	
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
	name = QUrl::fromPercentEncoding(name.toUtf8());
	m_strFile = (!name.isEmpty()) ? name : "default.html";
	m_bAutoName = true;
}

int CurlDownload::acceptable(QString uri, bool)
{
	QUrl url = uri;
	QString scheme = url.scheme();
	
	if(scheme != "http" && scheme != "ftp" && scheme != "https" && scheme != "sftp")
		return 0;
	else
		return 2;
}

void CurlDownload::globalInit()
{
	new CurlPoller;

	CurlPoller::setTransferTimeout(getSettingsValue("httpftp/timeout").toInt());
	
	SettingsItem si;
	si.icon = DelayedIcon(":/fatrat/httpftp.png");
	si.title = tr("HTTP/FTP");
	si.lpfnCreate = HttpFtpSettings::create;
	
	addSettingsPage(si);
}

void CurlDownload::globalExit()
{
	delete CurlPoller::instance();
}

void CurlDownload::setObject(QString target)
{
	QDir dirnew = target;
	if(dirnew != m_dir)
	{
		if(!QFile::rename(filePath(), target+"/"+m_strFile))
			throw RuntimeException(tr("Cannot move the file."));
			
		m_dir = dirnew;
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

int anti_crash_fun() { return 0; }

void CurlDownload::changeActive(bool bActive)
{
	if(bActive)
	{
		m_strMessage.clear();
		
		if(m_urls.isEmpty())
		{
			enterLogMessage(m_strMessage = tr("No URLs have been given"));
			setState(Failed);
			return;
		}
		
		if(m_nUrl >= m_urls.size())
			m_nUrl = 0;
		
		std::string spath = filePath().toStdString();
		m_file = open(spath.c_str(), O_CREAT|O_RDWR|O_LARGEFILE, 0666);
		if(m_file < 0)
		{
			enterLogMessage(m_strMessage = strerror(errno));
			setState(Failed);
			return;
		}
		lseek64(m_file, 0, SEEK_END);
		
		QByteArray ba, auth;
		QUrl url = m_urls[m_nUrl].url;
		bool bWatchHeaders = false;
		
		m_curl = curl_easy_init();
		
		if(!url.userInfo().isEmpty())
		{
			auth = url.userInfo().toUtf8();
			url.setUserInfo(QString());
		}
		
		ba = url.toEncoded();
		
		bWatchHeaders = ba.startsWith("http");
		
		curl_easy_setopt(m_curl, CURLOPT_URL, ba.constData());
		
		if(!auth.isEmpty())
			curl_easy_setopt(m_curl, CURLOPT_USERPWD, auth.constData());
		
		Proxy proxy = Proxy::getProxy(m_urls[m_nUrl].proxy);
		if(proxy.nType != Proxy::ProxyNone)
		{
			QByteArray p;
			
			if(proxy.strUser.isEmpty())
				p = QString("%1:%2").arg(proxy.strIP).arg(proxy.nPort).toLatin1();
			else
				p = QString("%1:%2@%3:%4").arg(proxy.strUser).arg(proxy.strPassword).arg(proxy.strIP).arg(proxy.nPort).toLatin1();
			curl_easy_setopt(m_curl, CURLOPT_PROXY, p.constData());
			
			int type;
			if(proxy.nType == Proxy::ProxySocks5)
				type = CURLPROXY_SOCKS5;
			else if(proxy.nType == Proxy::ProxyHttp)
				type = CURLPROXY_HTTP;
			else
				type = 0;
			curl_easy_setopt(m_curl, CURLOPT_PROXYTYPE, type);
		}
		else
			curl_easy_setopt(m_curl, CURLOPT_PROXY, "");
		
		ba = m_urls[m_nUrl].strBindAddress.toUtf8();
		if(!ba.isEmpty())
			curl_easy_setopt(m_curl, CURLOPT_INTERFACE, ba.constData());
		
		curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, true);
		curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, true);
		curl_easy_setopt(m_curl, CURLOPT_UNRESTRICTED_AUTH, true);
		curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
		curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
		curl_easy_setopt(m_curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);

		m_nStart = lseek64(m_file, 0, SEEK_CUR);
		curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, m_nStart);

		qDebug() << "Resume from" << m_nStart;

		curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_function);
		curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, static_cast<CurlUser*>(this));
		
		if(bWatchHeaders)
		{
			curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, process_header);
			curl_easy_setopt(m_curl, CURLOPT_WRITEHEADER, this);
		}
		
		curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_debug_callback);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(m_curl, CURLOPT_VERBOSE, true);
		curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
		curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, true);
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, false);
		curl_easy_setopt(m_curl, CURLOPT_SSH_AUTH_TYPES, CURLSSH_AUTH_PASSWORD | CURLSSH_AUTH_KEYBOARD);
		curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_NONE);
		
		int timeout = getSettingsValue("httpftp/timeout").toInt();
		curl_easy_setopt(m_curl, CURLOPT_FTP_RESPONSE_TIMEOUT, timeout);
		curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, timeout);
		
		curl_easy_setopt(m_curl, CURLOPT_SEEKFUNCTION, seek_function);
		curl_easy_setopt(m_curl, CURLOPT_SEEKDATA, &m_file);
		curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, true);
		
		if(getSettingsValue("httpftp/forbidipv6").toBool())
			curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
		
		// BUG (CRASH) WORKAROUND
		//curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, true); // this doesn't help
		curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, anti_crash_fun);
		
		CurlPoller::instance()->addTransfer(this);
	}
	else if(m_curl != 0)
	{
		resetStatistics();
		CurlPoller::instance()->removeTransfer(this);
		CurlPoller::instance()->addForSafeDeletion(m_curl);
		m_curl = 0;
		m_nStart = 0;
		qDebug() << "Closing at pos" << lseek64(m_file, 0, SEEK_CUR);
		close(m_file);
		m_file = 0;
	}
}

bool CurlDownload::writeData(const char* buffer, size_t bytes)
{
	if(!isActive())
		return false;
	
	if(m_curl /**&& (!m_nTotal || m_nTotal == -1LL || lseek(m_file, 0, SEEK_CUR) == 0)*/)
	{
		double len;
		curl_easy_getinfo(m_curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);
		//std::cout << "total = " << m_nStart << " + " << qlonglong(len) << "(was" << m_nTotal << ")";
		if(len != -1 && len != 0)
			m_nTotal = m_nStart + len;
	}
	
	if (write(m_file, buffer, bytes) != bytes)
	{
		m_strMessage = tr("Write failed");
		enterLogMessage(tr("Write failed (%1)").arg(strerror(errno)));
		return false;
	}
	
	return true;
}

size_t CurlDownload::process_header(const char* ptr, size_t size, size_t nmemb, CurlDownload* This)
{
	QByteArray line = QByteArray(ptr, size*nmemb).trimmed();
	int pos = line.indexOf(": ");
	
	if(pos != -1)
		This->m_headers[line.left(pos).toLower()] = line.mid(pos+2);
	if(line.isEmpty())
		This->processHeaders();
	
	return size*nmemb;
}

void CurlDownload::processHeaders()
{
	if(!m_headers.contains("location"))
	{
		if(m_headers.contains("content-disposition") && m_bAutoName)
		{
			QByteArray con = m_headers["content-disposition"];
			int pos = con.indexOf("filename=");
			
			if(pos != -1)
			{
				QString name = con.mid(pos+9);
				
				if(name.startsWith('"') && name.endsWith('"'))
					name = name.mid(1, name.size()-2);
				
				name.replace('/', '_');
				qDebug() << "Automatically renaming to" << name;
				name = QUrl::fromPercentEncoding(name.toUtf8());
				setTargetName(name);
			}
		}
	}
	else
	{
		QByteArray newurl = m_headers["location"];
		if(m_bAutoName)
		{
			QString name = QUrl::fromPercentEncoding(newurl);
			setTargetName(QFileInfo(newurl).fileName());
		}
	}
	
	m_headers.clear();
}

int CurlDownload::seek_function(int file, curl_off_t offset, int origin)
{
	qDebug() << "seek_function" << offset << origin;
	
	if (lseek(file, offset, origin) == (off_t) -1)
		return -1;
	
	return 0;
}

int CurlDownload::curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, CurlDownload* This)
{
	if(type != CURLINFO_DATA_IN && type != CURLINFO_DATA_OUT)
	{
		QByteArray line = QByteArray(text, bytes).trimmed();
		qDebug() << "CURL debug:" << line;
		if(!line.isEmpty())
			This->enterLogMessage(line);
	}
	
	return 0;
}

void CurlDownload::transferDone(CURLcode result)
{
	if(!isActive())
		return;
	
	if(result == CURLE_OK && (done() == total() || !total()))
		setState(Completed);
	else
	{
		m_strMessage = m_errorBuffer;
		m_strMessage += QString(" (%1)").arg(result);
		
		if(result == CURLE_OPERATION_TIMEDOUT)
			m_strMessage = tr("Timeout");
		
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
	CurlUser::speeds(down, up);
}

qulonglong CurlDownload::total() const
{
	return m_nTotal;
}

qulonglong CurlDownload::done() const
{
	if(!isActive())
	{
		if(m_nStart)
			return m_nStart;
		else
		{
			QFileInfo info(m_dir.filePath(name()));
			return info.exists() ? info.size() : 0;
		}
	}
	else
		return lseek64(m_file, 0, SEEK_CUR);
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
		
		setXMLProperty(doc, sub, "address", QString(m_urls[i].url.toEncoded()));
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

void CurlDownload::setSpeedLimits(int down, int)
{
	m_down.max = down;
}


QDialog* CurlDownload::createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers)
{
	HttpUrlOptsDlg* obj = new HttpUrlOptsDlg(parent, &transfers);
	obj->init();
	return obj;
}

WidgetHostChild* CurlDownload::createOptionsWidget(QWidget* w)
{
	return new HttpOptsWidget(w, this);
}

QString CurlDownload::remoteURI() const
{
	if (m_urls.isEmpty())
		return QString();
	if (m_nUrl >= m_urls.size())
		m_nUrl = 0;
	QUrl url = m_urls[m_nUrl].url;
	url.setUserInfo(QString());
	return url.toString();
}
