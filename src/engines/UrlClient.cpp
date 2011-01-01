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

#include "config.h"
#include "UrlClient.h"
#include "Proxy.h"
#include "fatrat.h"
#include "CurlPollingMaster.h"
#include "Settings.h"
#include <QFileInfo>
#include <cstring>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

UrlClient::UrlClient()
	: m_source(0), m_target(0), m_rangeFrom(0), m_rangeTo(-1), m_progress(0), m_curl(0), m_bTerminating(false)
{
	m_errorBuffer[0] = 0;
}

UrlClient::~UrlClient()
{
	if (m_target)
	{
		close(m_target);
		m_target = 0;
	}
//	if (m_curl != 0)
//		CurlPoller::instance()->removeSafely(m_curl);
}

void UrlClient::setSourceObject(UrlObject& obj)
{
	m_source = &obj;
}

void UrlClient::setTargetObject(int fd)
{
	m_target = fd;
}

void UrlClient::setRange(qlonglong from, qlonglong to)
{
	m_rangeFrom = from;
	m_rangeTo = to;
}

qlonglong UrlClient::progress() const
{
	return m_progress;
}

int anti_crash_fun();

void UrlClient::start()
{
	QByteArray ba, auth;
	QUrl url = m_source->url;
	bool bWatchHeaders = false;
	
	if(lseek64(m_target, m_rangeFrom, SEEK_SET) == (off_t) -1)
	{
		emit failure(tr("Failed to seek in the file - %1").arg(strerror(errno)));
		return;
	}
	qDebug() << "Position in file:" << lseek64(m_target, 0, SEEK_CUR);
	
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

	if (!m_source->cookies.isEmpty())
	{
		QByteArray ba;
		foreach (QNetworkCookie c, m_source->cookies)
		{
			if (!ba.isEmpty())
				ba += "; ";
			ba += c.toRawForm(QNetworkCookie::NameAndValueOnly);
		}
		curl_easy_setopt(m_curl, CURLOPT_COOKIE, ba.constData());
	}
	
	Proxy proxy = Proxy::getProxy(m_source->proxy);
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
		
	ba = m_source->strBindAddress.toUtf8();
	if(!ba.isEmpty())
		curl_easy_setopt(m_curl, CURLOPT_INTERFACE, ba.constData());
	
	if(getSettingsValue("httpftp/forbidipv6").toInt() != 0)
		curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	
	curl_easy_setopt(m_curl, CURLOPT_AUTOREFERER, true);
	curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, true);
	curl_easy_setopt(m_curl, CURLOPT_UNRESTRICTED_AUTH, true);
	curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
	curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
	curl_easy_setopt(m_curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);

	if(m_rangeFrom || m_rangeTo != -1)
	{
		char range[128];
		if(m_rangeTo != -1)
			snprintf(range, sizeof(range), "%lld-%lld", m_rangeFrom, m_rangeTo-1);
		else
			snprintf(range, sizeof(range), "%lld-", m_rangeFrom);

		curl_easy_setopt(m_curl, CURLOPT_RANGE, range);
	}
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
	
	int timeout = getSettingsValue("httpftp/timeout").toInt();
	curl_easy_setopt(m_curl, CURLOPT_FTP_RESPONSE_TIMEOUT, timeout);
	curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, timeout);
	curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, true);
	
	//curl_easy_setopt(m_curl, CURLOPT_SEEKFUNCTION, seek_function);
	//curl_easy_setopt(m_curl, CURLOPT_SEEKDATA, &m_file);
	
	// BUG (CRASH) WORKAROUND
	curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, true); // this doesn't help
	curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, anti_crash_fun);
}

void UrlClient::stop()
{
	//m_master->removeTransfer(this);
	//curl_easy_cleanup(m_curl);
	//m_curl = 0;
	m_bTerminating = true;
	m_progress = 0;
}

size_t UrlClient::process_header(const char* ptr, size_t size, size_t nmemb, UrlClient* This)
{
	QByteArray line = QByteArray(ptr, size*nmemb).trimmed();
	int pos = line.indexOf(": ");
	
	if(pos != -1)
		This->m_headers[line.left(pos).toLower()] = line.mid(pos+2);
	if(line.isEmpty())
		This->processHeaders();
	
	return size*nmemb;
}

void UrlClient::processHeaders()
{
	if(!m_headers.contains("location"))
	{
		if(m_headers.contains("content-disposition") /*&& m_bAutoName*/)
		{
			QByteArray con = m_headers["content-disposition"];
			int pos = con.indexOf("filename=");
			
			if(pos != -1)
			{
				QString name = con.mid(pos+9);
				
				QRegExp quoted("\"([^\"]+)\".*");
				if(quoted.exactMatch(name))
					name = quoted.cap(1);
				
				name.replace('/', '_');
				qDebug() << "Automatically renaming to" << name;
				//setTargetName(name);
				emit renameTo(name);
			}
		}
	}
	else
	{
		QByteArray newurl = m_headers["location"];
		//if(m_bAutoName)
		//	setTargetName(QFileInfo(newurl).fileName());
		emit renameTo(QFileInfo(newurl).fileName());
	}
	
	m_headers.clear();
}

CURL* UrlClient::curlHandle()
{
	return m_curl;
}

int UrlClient::curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, UrlClient* This)
{
	if(type != CURLINFO_DATA_IN && type != CURLINFO_DATA_OUT)
	{
		QByteArray line = QByteArray(text, bytes).trimmed();
		qDebug() << "CURL debug:" << line;
		if(!line.isEmpty())
			emit This->logMessage(line);
	}
	
	return 0;
}

bool UrlClient::writeData(const char* buffer, size_t bytes)
{
	int towrite = int(bytes);
	if(m_bTerminating)
		return true;
	
	if(m_rangeTo == -1)
	{
		double len;
		//long code;

		//curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
		//if (code/100 == 2)
		{
			curl_easy_getinfo(m_curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &len);
			if(len > 0)
			{
				m_rangeTo = m_rangeFrom + len;
				emit totalSizeKnown(m_rangeTo);
			}
		}
	}
	if(!m_progress)
	{
		char url[1024];
		if (curl_easy_getinfo(m_curl, CURLINFO_EFFECTIVE_URL, url) == CURLE_OK)
		{
			QString surl = m_source->url.toString();
			QString enc = m_source->url.toEncoded();
			if (surl.compare(QLatin1String(url)) && enc.compare(QLatin1String(url)))
				m_source->effective = url;
		}
	}
	
	if(m_rangeTo != -1)
	{
		qlonglong maximum = m_rangeTo - m_rangeFrom - m_progress;
		if (maximum < 0)
			maximum = 0;
		if(qlonglong(towrite) > maximum)
			towrite = maximum;
	}
	
	if (towrite > 0)
	{
		if (write(m_target, buffer, towrite) != towrite && !m_bTerminating)
		{
			emit failure(tr("Write failed (%1)").arg(strerror(errno)));
			m_bTerminating = true;
			return false;
		}
	}

	if(m_progress+qlonglong(bytes) > m_rangeTo-m_rangeFrom && m_rangeTo != -1 && !m_bTerminating)
	{
		// The range has apparently been shrinked since the thread was started
		qDebug() << "----------- Prematurely ending a shortened segment - m_rangeTo:" << m_rangeTo << "; progress:" << (m_progress+bytes);
		m_bTerminating = true;
		emit done(QString());
	}
	m_progress += towrite;
	
	//if(m_master != 0)
	//	m_master->timeProcessDown(towrite);
	
	return true;
}

void UrlClient::transferDone(CURLcode result)
{
	if (m_bTerminating)
		return;

	qDebug() << "UrlClient::transferDone" << result;
	curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, 0);
	if(result == CURLE_OK || (m_rangeFrom + m_progress >= m_rangeTo && m_rangeTo > 0))
	{
		qDebug() << "Segment finished, rangeTo:" << m_rangeTo;
		m_bTerminating = true;
		emit done(QString());
	}
	else if (result == CURLE_RANGE_ERROR)
	{
		emit rangesUnsupported();
	}
	else
	{
		QString err;
		if(result == CURLE_OPERATION_TIMEDOUT)
			err = tr("Timeout");
		else if(m_errorBuffer[0])
			err = QString::fromUtf8(m_errorBuffer);
		else
			err = curl_easy_strerror(result);
		qDebug() << "The transfer has failed, firing an event";
		m_bTerminating = true;
		emit done(err);
	}
}

void UrlClient::setPollingMaster(CurlPollingMaster* master)
{
	m_master = master;
}


