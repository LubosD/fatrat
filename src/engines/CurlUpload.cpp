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

#include "CurlUpload.h"
#include "CurlPoller.h"
#include "RuntimeException.h"
#include "tools/HashDlg.h"
#include "Proxy.h"
#include "Auth.h"
#include <QFileInfo>
#include <QMenu>

CurlUpload::CurlUpload()
	: m_curl(0), m_nDone(0), m_nTotal(0), m_mode(FtpPassive), m_curlUser(this)
{
	Transfer::m_mode = Upload;
	m_errorBuffer[0] = 0;
}

CurlUpload::~CurlUpload()
{
	if(isActive())
		changeActive(false);
}

void CurlUpload::init(QString source, QString target)
{
	QFileInfo finfo;
	
	if(source.startsWith("file:///"))
		source = source.mid(7);
	
	finfo = QFileInfo(source);
	
	if(!finfo.exists())
		throw RuntimeException(tr("File does not exist"));
	
	m_nTotal = finfo.size();
	if(m_strName.isEmpty())
		m_strName = finfo.fileName();
	
	if(!target.startsWith("ftp://") && !target.startsWith("sftp://"))
		throw RuntimeException(tr("Invalid protocol for this upload class (FTP)"));
	
	m_strTarget = target;
	m_strSource = source;
	
	if(m_strTarget.userInfo().isEmpty())
	{
		QList<Auth> auths = Auth::loadAuths();
		foreach(Auth a,auths)
		{
			if(!QRegExp(a.strRegExp).exactMatch(target))
				continue;
			
			m_strTarget.setUserName(a.strUser);
			m_strTarget.setPassword(a.strPassword);
			
			enterLogMessage(tr("Loaded stored authentication data, matched regexp %1").arg(a.strRegExp));
			
			break;
		}
	}
}

void CurlUpload::setObject(QString source)
{
	m_strSource = source;
}

int CurlUpload::seek_function(QFile* file, curl_off_t offset, int origin)
{
	qDebug() << "seek_function" << offset << origin;
	
	if(origin == SEEK_SET)
	{
		if(!file->seek(offset))
			return -1;
	}
	else if(origin == SEEK_CUR)
	{
		if(!file->seek(file->pos() + offset))
			return -1;
	}
	else if(origin == SEEK_END)
	{
		if(!file->seek(file->size() + offset))
			return -1;
	}
	else
		return -1;
	return 0;
}

int anti_crash_fun();

void CurlUpload::changeActive(bool nowActive)
{
	if(nowActive)
	{
		m_file.setFileName(m_strSource);
		if(!m_file.open(QIODevice::ReadOnly))
		{
			enterLogMessage(m_strMessage = m_file.errorString());
			setState(Failed);
			return;
		}
		
		m_nTotal = m_file.size();
		
		m_curl = curl_easy_init();
		curl_easy_setopt(m_curl, CURLOPT_UPLOAD, true);
		curl_easy_setopt(m_curl, CURLOPT_INFILESIZE_LARGE, total());
		curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, -1LL);
		curl_easy_setopt(m_curl, CURLOPT_INFILESIZE_LARGE, m_nTotal);
		curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
		curl_easy_setopt(m_curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
		curl_easy_setopt(m_curl, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_SINGLECWD);
		curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, anti_crash_fun);
		
		curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, false);
		
		curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, CurlUser::read_function);
		curl_easy_setopt(m_curl, CURLOPT_READDATA, &m_curlUser);
		
		curl_easy_setopt(m_curl, CURLOPT_SEEKFUNCTION, seek_function);
		curl_easy_setopt(m_curl, CURLOPT_SEEKDATA, &m_file);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_debug_callback);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(m_curl, CURLOPT_VERBOSE, true);
		
		curl_easy_setopt(m_curl, CURLOPT_FTP_RESPONSE_TIMEOUT, 10);
		curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10);
		
		{
			QByteArray ba;
			ba = m_strTarget.toString().toUtf8();
			if(!ba.endsWith("/"))
				ba += '/';
			int end = m_strSource.lastIndexOf('/');
			
			if(end < 0)
				ba += m_strSource.toUtf8();
			else
				ba += m_strSource.mid(end+1).toUtf8();
			curl_easy_setopt(m_curl, CURLOPT_URL, ba.constData());
		}
		{
			QByteArray ba = m_strBindAddress.toUtf8();
			if(!ba.isEmpty())
				curl_easy_setopt(m_curl, CURLOPT_INTERFACE, ba.constData());
		}
		
		if(m_mode == FtpActive)
			curl_easy_setopt(m_curl, CURLOPT_FTPPORT, "-");
		
		Proxy proxy = Proxy::getProxy(m_proxy);
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
		
		CurlPoller::instance()->addTransfer(&m_curlUser);
	}
	else
	{
		m_nDone = done();
		
		m_curlUser.resetStatistics();
		CurlPoller::instance()->removeTransfer(&m_curlUser);
		curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, 0);
		curl_easy_cleanup(m_curl);
		m_curl = 0;
		m_file.close();
	}
}

size_t CurlUpload::readData(char* buffer, size_t maxData)
{
	return m_file.read(buffer, maxData);
}

int CurlUpload::curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, CurlUpload* This)
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

void CurlUpload::setSpeedLimits(int, int up)
{
	m_curlUser.setMaxUp(up);
}

void CurlUpload::speeds(int& down, int& up) const
{
	m_curlUser.speeds(down, up);
}

qulonglong CurlUpload::done() const
{
	if(!m_curl)
		return m_nDone;
	else
	{
		double totalBytes, doneBytes;
		curl_easy_getinfo(m_curl, CURLINFO_CONTENT_LENGTH_UPLOAD, &totalBytes);
		curl_easy_getinfo(m_curl, CURLINFO_SIZE_UPLOAD, &doneBytes);
		
		//qDebug() << "CURLINFO_CONTENT_LENGTH_UPLOAD" << totalBytes;
		//qDebug() << "CURLINFO_SIZE_UPLOAD" << doneBytes;
		
		if(totalBytes)
			return total() - totalBytes + doneBytes;
		else
			return m_nDone;
	}
}

void CurlUpload::load(const QDomNode& map)
{
	QString source, target;
	Transfer::load(map);
	
	source = getXMLProperty(map, "source");
	target = getXMLProperty(map, "target");
	m_strName = getXMLProperty(map, "name");
	m_mode = (FtpMode) getXMLProperty(map, "ftpmode").toInt();
	m_nDone = getXMLProperty(map, "done").toLongLong();
	m_proxy = getXMLProperty(map, "proxy");
	m_strBindAddress = getXMLProperty(map, "bindaddr");
	
	try
	{
		init(source, target);
	}
	catch(const RuntimeException& e)
	{
		setState(Failed);
		m_strMessage = e.what();
	}
}

void CurlUpload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "source", m_strSource);
	setXMLProperty(doc, map, "target", m_strTarget.toString());
	setXMLProperty(doc, map, "name", m_strName);
	setXMLProperty(doc, map, "ftpmode", QString::number(m_mode));
	setXMLProperty(doc, map, "done", QString::number(m_nDone));
	setXMLProperty(doc, map, "proxy", m_proxy.toString());
	setXMLProperty(doc, map, "bindaddr", m_strBindAddress);
}

int CurlUpload::acceptable(QString url, bool bDrop)
{
	if(bDrop)
		return (url.startsWith("file://") || url.startsWith("/")) ? 2 : 0;
	else
		return (url.startsWith("ftp://") || url.startsWith("sftp://")) ? 2 : 0;
}

CURL* CurlUpload::curlHandle()
{
	return m_curl;
}

void CurlUpload::transferDone(CURLcode result)
{
	if(!isActive())
		return;
	
	if(result == CURLE_OK || done() == total())
		setState(Completed);
	else
	{
		if(result == CURLE_OPERATION_TIMEDOUT)
			strcpy(m_errorBuffer, "Timeout");
		
		m_strMessage = m_errorBuffer;
		setState(Failed);
	}
}

WidgetHostChild* CurlUpload::createOptionsWidget(QWidget* w)
{
	return new FtpUploadOptsForm(w, this);
}

void CurlUpload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Compute hash..."));
	connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
}

void CurlUpload::computeHash()
{
	HashDlg dlg(getMainWindow(), m_strSource);
	dlg.exec();
}

///////////////////////////////////////////

FtpUploadOptsForm::FtpUploadOptsForm(QWidget* me, CurlUpload* myobj)
	: m_upload(myobj)
{
	setupUi(me);
	
	if(myobj->m_strTarget.scheme() != "ftp")
	{
		//comboFtpMode->setDisabled(true);
		comboProxy->setDisabled(true);
	}
}

void FtpUploadOptsForm::load()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl temp, url;
	
	temp = url = m_upload->m_strTarget;
	temp.setUserInfo(QString());
	lineTarget->setText(temp.toString());
	lineUsername->setText(url.userName());
	linePassword->setText(url.password());
	
	comboFtpMode->addItems(QStringList(tr("Active mode")) << tr("Passive mode"));
	comboFtpMode->setCurrentIndex(int(m_upload->m_mode));
	
	comboProxy->addItem(tr("(none)", "No proxy"));
	comboProxy->setCurrentIndex(0);
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].strName);
		if(listProxy[i].uuid == m_upload->m_proxy)
			comboProxy->setCurrentIndex(i+1);
	}
	
	lineAddrBind->setText(m_upload->m_strBindAddress);
}

void FtpUploadOptsForm::accepted()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl url = lineTarget->text();
	
	url.setUserName(lineUsername->text());
	url.setPassword(linePassword->text());
	
	m_upload->m_strTarget = url.toString();
	int ix = comboProxy->currentIndex();
	m_upload->m_proxy = (!ix) ? QUuid() : listProxy[ix-1].uuid;
	
	m_upload->m_mode = FtpMode( comboFtpMode->currentIndex() );
	m_upload->m_strBindAddress = lineAddrBind->text();
}

bool FtpUploadOptsForm::accept()
{
	bool acc = false;
	
	acc |= lineTarget->text().startsWith("ftp://");
	acc |= lineTarget->text().startsWith("sftp://");
	
	return acc;
}

