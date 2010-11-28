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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "Proxy.h"
#include "RapidshareUpload.h"
#include "RuntimeException.h"
#include "CurlPoller.h"
#include "WidgetHostDlg.h"
#include "Settings.h"
#include "RapidshareUploadDetails.h"
#include "RapidshareStatusWidget.h"
#include "fatrat.h"
#include "Auth.h"

#include <QHttp>
#include <QBuffer>
#include <QThread>
#include <QFileInfo>
#include <QSettings>
#include <QFileDialog>
#include <QtDebug>

extern QSettings* g_settings;

RapidshareStatusWidget* RapidshareUpload::m_labelStatus = 0;
const long CHUNK_SIZE = 10*1024*1024;

RapidshareUpload::RapidshareUpload()
	: m_curl(0), m_postData(0), m_http(0), m_buffer(0)
{
	m_query = QueryNone;
	m_nFileID = -1;
	m_nDone = 0;
	m_bIDJustChecked = false;
	
	m_mode = Upload;
	m_buffer = new QBuffer;
	m_buffer->open(QIODevice::ReadWrite);
	
	m_type = AccountType( g_settings->value("rapidshare/account").toInt() );
	m_strUsername = g_settings->value("rapidshare/username").toString();
	m_strPassword = g_settings->value("rapidshare/password").toString();
	
	m_strLinksDownload = g_settings->value("rapidshare/down_links").toString();
	m_strLinksKill = g_settings->value("rapidshare/kill_links").toString();
}

RapidshareUpload::~RapidshareUpload()
{
	delete m_buffer;
	
	if(m_curl)
		curl_easy_cleanup(m_curl);
	if(m_postData)
		curl_formfree(m_postData);
}

void RapidshareUpload::globalInit()
{
	SettingsItem si;
	
	si.icon = DelayedIcon(":/fatrat/rscom.png");
	si.title = tr("RapidShare.com");
	si.lpfnCreate = RapidshareSettings::create;
	
	addSettingsPage(si);
	applySettings();
}

void RapidshareUpload::applySettings()
{
	if(programHasGUI())
	{
		if(getSettingsValue("rapidshare/status_widget").toBool())
		{
			if(!m_labelStatus)
			{
				m_labelStatus = new RapidshareStatusWidget;
				addStatusWidget(m_labelStatus, true);
			}
			else
				m_labelStatus->applySettings();
		}
		else if(m_labelStatus)
		{
			removeStatusWidget(m_labelStatus);
			m_labelStatus = 0;
		}
	}
}

int RapidshareUpload::acceptable(QString url, bool)
{
	return 0; // because this class doesn't actually use URLs
}

int anti_crash_fun();

void RapidshareUpload::init(QString source, QString target)
{
	setObject(source);
	
	m_proxy = getSettingsValue("rapidshare/proxy").toString();
}

void RapidshareUpload::curlInit()
{
	if(m_curl)
		curl_easy_cleanup(m_curl);
	
	m_curl = curl_easy_init();
	//curl_easy_setopt(m_curl, CURLOPT_POST, true);
	if(getSettingsValue("httpftp/forbidipv6").toInt() != 0)
		curl_easy_setopt(m_curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "FatRat/" VERSION);
	curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER, m_errorBuffer);
	curl_easy_setopt(m_curl, CURLOPT_SEEKFUNCTION, seek_function);
	curl_easy_setopt(m_curl, CURLOPT_SEEKDATA, this);
	curl_easy_setopt(m_curl, CURLOPT_DEBUGDATA, this);
	curl_easy_setopt(m_curl, CURLOPT_VERBOSE, true);
	curl_easy_setopt(m_curl, CURLOPT_PROGRESSFUNCTION, anti_crash_fun);
	curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, CurlUser::write_function);
	curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, static_cast<CurlUser*>(this));
	curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, CurlUser::read_function);
	curl_easy_setopt(m_curl, CURLOPT_READDATA, static_cast<CurlUser*>(this));
	curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
}

CURL* RapidshareUpload::curlHandle()
{
	return m_curl;
}

size_t RapidshareUpload::readData(char* buffer, size_t maxData)
{
	maxData = qMin<size_t>(m_nDone+CHUNK_SIZE - m_file.pos(), maxData);
	return m_file.read(buffer, maxData);
}

bool RapidshareUpload::writeData(const char* buffer, size_t bytes)
{
	m_buffer->write(buffer, bytes);
	return true;
}

int RapidshareUpload::curl_debug_callback(CURL*, curl_infotype type, char* text, size_t bytes, RapidshareUpload* This)
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


void RapidshareUpload::setObject(QString source)
{
	m_strSource = source;
	
	int slash = source.lastIndexOf('/');
	if(slash < 0)
		m_strName = source;
	else
		m_strName = source.mid(slash+1);
	m_nTotal = QFileInfo(m_strSource).size();
}

void RapidshareUpload::changeActive(bool nowActive)
{
	if(nowActive)
	{
		curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, curl_debug_callback);
		
		if(m_type != AccountNone && (m_strUsername.isEmpty() || m_strPassword.isEmpty()))
		{
			m_strMessage = tr("You have to enter your account information");
			setState(Failed);
			return;
		}
		
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
		
		if(!m_file.isOpen())
		{
			m_file.setFileName(m_strSource);
			if(!m_file.open(QIODevice::ReadOnly))
			{
				m_strMessage = tr("Can't open the file");
				setState(Failed);
				return;
			}
		}
		
		delete m_buffer;
		m_buffer = new QBuffer;
		m_buffer->open(QIODevice::ReadWrite);
		
		if(m_strServer.isEmpty())
		{
			m_query = QueryServerID;
			m_http = new QHttp("rapidshare.com", 80);
			
			connect(m_http, SIGNAL(done(bool)), this, SLOT(queryDone(bool)));
			
			m_http->setProxy(Proxy::getProxy(m_proxy));
			m_http->get("/cgi-bin/rsapi.cgi?sub=nextuploadserver_v1", m_buffer);
		}
		else
		{
			if(m_nFileID > 0 && !m_strKillID.isEmpty() && !m_bIDJustChecked)
			{
				if(m_nDone < m_nTotal)
				{
					// verify that the ID is still valid
					m_query = QueryFileInfo;
					m_http = new QHttp("rapidshare.com", 80);
					
					connect(m_http, SIGNAL(done(bool)), this, SLOT(queryDone(bool)));
					
					m_http->setProxy(Proxy::getProxy(m_proxy));
					m_http->get(QString("/cgi-bin/rsapi.cgi?sub=checkincomplete_v1&fileid=%1&killcode=%2").arg(m_nFileID).arg(m_strKillID), m_buffer);
				}
				else
					setState(Completed);
			}
			else
			{
				if(m_nFileID <= 0 || m_strKillID.isEmpty())
					m_nDone = 0;
				
				m_nTotal = QFileInfo(m_strSource).size();
				
				if((m_nTotal > 200*1024*1024 && m_type != AccountPremium) || m_nTotal > 2LL*1024LL*1024LL*1024LL)
				{
					m_strMessage = tr("The maximum file size is 200 MB/2 GB (premium)");
					setState(Failed);
					return;
				}
				
				if(m_nDone < m_nTotal)
					beginNextChunk();
				else
					setState(Completed);
			}
		}
	}
	else
	{
		m_bIDJustChecked = false;
		if(m_http)
		{
			m_http->abort();
			m_http->deleteLater();
			m_http = 0;
		}
		
		CurlPoller::instance()->removeTransfer(this);
		resetStatistics();
		
		if(m_curl)
		{
			curl_easy_setopt(m_curl, CURLOPT_DEBUGFUNCTION, 0);
			curl_easy_cleanup(m_curl);
			m_curl = 0;
		}
		
		if(m_postData)
		{
			curl_formfree(m_postData);
			m_postData = 0;
		}
	}
}

int RapidshareUpload::seek_function(RapidshareUpload* This, curl_off_t offset, int origin)
{
	qDebug() << "seek_function" << offset << origin;
	
	if(origin == SEEK_SET)
	{
		if(!This->m_file.seek(This->m_nDone + offset))
			return -1;
	}
	else if(origin == SEEK_CUR)
	{
		if(!This->m_file.seek(This->m_file.pos() + offset))
			return -1;
	}
	else
		return -1;
	return 0;
}

void RapidshareUpload::beginNextChunk()
{
	curl_httppost* lastData = 0;
	const char* handler;
	const char* state = 0;
	
	if(m_postData)
	{
		curl_formfree(m_postData);
		m_postData = 0;
	}
	
	curlInit();
	curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, "rsapi_v1", CURLFORM_PTRCONTENTS, "1", CURLFORM_END);
	
	//qDebug() << "***FileID" << m_nFileID;
	//qDebug() << "***KillID" << m_nKillID;
	//qDebug() << "***Done" << m_nDone;
	
	if(m_nFileID > 0 && !m_strKillID.isEmpty() && m_nDone > 0)
	{
		char buf[50];
		
		sprintf(buf, "%lld", m_nFileID);
		curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, "fileid", CURLFORM_COPYCONTENTS, buf, CURLFORM_END);
		curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, "killcode", CURLFORM_COPYCONTENTS, m_strKillID.toLatin1().data(), CURLFORM_END);
		
		handler = "uploadresume";
	}
	else
	{
		handler = "upload";
	
		if(m_type != AccountNone)
		{
			const char* acctype = (m_type == AccountCollector) ? "freeaccountid" : "login";
			curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, acctype, CURLFORM_COPYCONTENTS, m_strUsername.toUtf8().constData(), CURLFORM_END);
			curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, "password", CURLFORM_COPYCONTENTS, m_strPassword.toUtf8().constData(), CURLFORM_END);
		}
	}

	if(m_nDone+CHUNK_SIZE >= m_nTotal)
		state = "complete";
	else //if(m_nDone+CHUNK_SIZE < m_nTotal)
		state = "incomplete";
	
	if(state != 0)
		curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, state, CURLFORM_PTRCONTENTS, "1", CURLFORM_END);
	
	QString name;
	
	curl_formadd(&m_postData, &lastData, CURLFORM_PTRNAME, "filecontent",
		CURLFORM_STREAM, static_cast<CurlUser*>(this),
		CURLFORM_FILENAME, m_strName.toUtf8().constData(),
		CURLFORM_CONTENTSLENGTH, qMin<long>(CHUNK_SIZE, m_nTotal - m_nDone), CURLFORM_END);
	
	QByteArray url = QString("http://%1/cgi-bin/%2.cgi").arg(m_strServer).arg(handler).toLatin1();
	curl_easy_setopt(m_curl, CURLOPT_URL, url.constData());
	curl_easy_setopt(m_curl, CURLOPT_HTTPPOST, m_postData);
	
	m_file.seek(m_nDone);
	
	delete m_buffer;
	m_buffer = new QBuffer;
	m_buffer->open(QIODevice::ReadWrite);
	
	CurlPoller::instance()->addTransfer(this);
}

void RapidshareUpload::transferDone(CURLcode result)
{
	if(result != CURLE_OK && isActive())
	{
		setState(Failed);
	}
	if(result == CURLE_OK)
	{
		m_nDone += qMin<qint64>(CHUNK_SIZE, m_nTotal - m_nDone);
		
		QRegExp reFileID("File1.1=http://rapidshare.com/files/(\\d+)/([^\\n]+)");
		QRegExp reKillID("\\?killcode=(\\d+)");
		QString link;
		const QByteArray& response = m_buffer->buffer();
		
		qDebug() << response;
		
		try
		{
			if(response.indexOf("forbiddenfiles=1") != -1)
				throw RuntimeException(tr("This file is forbidden to be shared"));
			
			if(response.indexOf("not found at RapidShare.com") != -1)
				throw RuntimeException(tr("Invalid username"));
			
			if(response.indexOf("Account found, but password is incorrect") != -1)
				throw RuntimeException(tr("Invalid password"));
			
			if(response.startsWith("ERROR"))
			{
				int ix = response.indexOf('.');
				if(ix > 0)
					throw RuntimeException(response.left(ix+1));
				else
					throw RuntimeException(response);
			}
			if(!response.startsWith("COMPLETE") && !response.startsWith("CHUNK"))
			{
				int ix = reFileID.indexIn(response);
				if(ix < 0)
					throw RuntimeException(tr("Failed to find the file ID"));
				m_nFileID = reFileID.cap(1).toLongLong();
				m_strSafeName = reFileID.cap(2);
				
				if(m_strSafeName.endsWith(".html"))
					m_strSafeName.resize(m_strSafeName.size()-5);
				
				ix = reKillID.indexIn(response);
				if(ix < 0)
					throw RuntimeException(tr("Failed to find the kill ID"));
				m_strKillID = reKillID.cap(1);
			}
			
			if(m_nDone == m_nTotal)
			{
				link = QString("http://rapidshare.com/files/%1/%2").arg(m_nFileID).arg(m_strSafeName);
				enterLogMessage(tr("Download link:") + ' ' + link);
				
				m_strMessage.clear();
				if(!m_strLinksDownload.isEmpty())
					saveLink(m_strLinksDownload, link);
				
				if(m_type == AccountNone)
				{
					link += QString("?killcode=%1").arg(m_strKillID);
					enterLogMessage(tr("Kill link:") + ' ' + link);
					if(!m_strLinksKill.isEmpty())
						saveLink(m_strLinksKill, link);
				}
				
				setState(Completed);
			}
			else if(isActive())
			{
				beginNextChunk();
			}
		}
		catch(const RuntimeException& e)
		{
			m_nDone = 0;
			m_strMessage = e.what();
			setState(Failed);
		}
	}
}

void RapidshareUpload::saveLink(QString filename, QString link)
{
	QFile file(filename);
	if(!file.open(QIODevice::Append))
	{
		enterLogMessage(tr("Cannot append to file \"%1\"").arg(filename));
	}
	else
	{
		file.write( (link + '\n').toUtf8() );
	}
}

void RapidshareUpload::queryDone(bool error)
{
	m_buffer->seek(0);
	
	if(m_query == QueryServerID)
	{
		if(error)
		{
			m_strMessage = tr("Failed to get server ID");
			setState(Failed);
		}
		else
		{
			m_strServer = QString("rs%1l3.rapidshare.com").arg(m_buffer->data().toInt());
			
			enterLogMessage(tr("Uploading to %1").arg(m_strServer));
		}
	}
	else if(m_query == QueryFileInfo)
	{
		if(error)
		{
			m_strMessage = tr("Failed to query resume information");
			setState(Failed);
		}
		else
		{
			m_bIDJustChecked = true;
			
			bool ok;
			m_nDone = m_buffer->data().toLongLong(&ok);
			
			if(!ok)
			{
				qDebug() << m_buffer->data() << "isn't a valid number";
				// file ID invalid etc.
				m_strMessage = m_buffer->data();
				m_nDone = 0;
				m_nFileID = -1;
				m_strKillID.clear();
				//setState(Failed);
			}
			else
				m_strMessage = tr("File ID %1 validated").arg(m_nFileID);
		}
	}
	
	m_query = QueryNone;
	m_http->deleteLater();
	m_http = 0;
	
	if(!error && isActive())
		changeActive(true); // next stage
}

void RapidshareUpload::setSpeedLimits(int, int up)
{
	m_up.max = up;
}

void RapidshareUpload::speeds(int& down, int& up) const
{
	up = down = 0;
	if(isActive())
		CurlUser::speeds(down, up);
}

qulonglong RapidshareUpload::done() const
{
	bool active = isActive();
	if(!active || (active && m_query == QueryFileInfo))
		return m_nDone;
	else
		return m_file.pos();
}

void RapidshareUpload::load(const QDomNode& map)
{
	setObject( getXMLProperty(map, "source") );
	m_nDone = getXMLProperty(map, "done").toLongLong();
	m_nFileID = getXMLProperty(map, "fileID").toLongLong();
	m_strKillID = getXMLProperty(map, "killID");
	m_type = (AccountType) getXMLProperty(map, "account").toLongLong();
	m_strUsername = getXMLProperty(map, "username");
	m_strPassword = getXMLProperty(map, "password");
	m_strServer = getXMLProperty(map, "server");
	m_strSafeName = getXMLProperty(map, "safename");
	m_proxy = getXMLProperty(map, "proxy");
	
	Transfer::load(map);
}

void RapidshareUpload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "source", m_strSource);
	setXMLProperty(doc, map, "done", QString::number(m_nDone));
	setXMLProperty(doc, map, "fileID", QString::number(m_nFileID));
	setXMLProperty(doc, map, "killID", m_strKillID);
	setXMLProperty(doc, map, "account", QString::number(int(m_type)));
	setXMLProperty(doc, map, "username", m_strUsername);
	setXMLProperty(doc, map, "password", m_strPassword);
	setXMLProperty(doc, map, "server", m_strServer);
	setXMLProperty(doc, map, "safename", m_strSafeName);
	setXMLProperty(doc, map, "proxy", m_proxy);
}

WidgetHostChild* RapidshareUpload::createOptionsWidget(QWidget* w)
{
	return new RapidshareOptsWidget(w, this);
}

QDialog* RapidshareUpload::createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers)
{
	WidgetHostDlg* host = new WidgetHostDlg(parent);
	host->addChild(new RapidshareOptsWidget(host->getChildHost(), &transfers, host));
	
	return host;
}

QObject* RapidshareUpload::createDetailsWidget(QWidget* widget)
{
	return new RapidshareUploadDetails(widget, this);
}

qint64 RapidshareUpload::chunkSize()
{
	return CHUNK_SIZE;
}

////////////////////////////////////////////

RapidshareOptsWidget::RapidshareOptsWidget(QWidget* me, RapidshareUpload* myobj)
	: m_upload(myobj), m_multi(0)
{
	init(me);
}

RapidshareOptsWidget::RapidshareOptsWidget(QWidget* me, QList<Transfer*>* multi, QObject* parent)
	: QObject(parent), m_upload(0), m_multi(multi)
{
	init(me);
	checkStatus->setVisible(false);
}

void RapidshareOptsWidget::init(QWidget* me)
{
	setupUi(me);
	connect(comboAccount, SIGNAL(currentIndexChanged(int)), this, SLOT(accTypeChanged(int)));
	connect(toolDownload, SIGNAL(clicked()), this, SLOT(browseDownload()));
	connect(toolKill, SIGNAL(clicked()), this, SLOT(browseKill()));
	
	comboAccount->addItems( QStringList() << tr("No account") << tr("Collector's account") << tr("Premium account") );
}

void RapidshareOptsWidget::load()
{
	int acc;
	if(m_upload)
	{
		acc = (int) m_upload->m_type;
		lineUsername->setText(m_upload->m_strUsername);
		linePassword->setText(m_upload->m_strPassword);
		lineLinksDownload->setText(m_upload->m_strLinksDownload);
		lineLinksKill->setText(m_upload->m_strLinksKill);
	}
	else
	{
		acc = g_settings->value("rapidshare/account").toInt();
		lineUsername->setText(g_settings->value("rapidshare/username").toString());
		linePassword->setText(g_settings->value("rapidshare/password").toString());
		lineLinksDownload->setText(g_settings->value("rapidshare/down_links").toString());
		lineLinksKill->setText(g_settings->value("rapidshare/kill_links").toString());
	}
	
	comboAccount->setCurrentIndex(acc);
	accTypeChanged(acc);
	
	QUuid myproxy;
	QList<Proxy> listProxy = Proxy::loadProxys();
	
	if(m_multi)
		myproxy = g_settings->value("rapidshare/proxy").toString();
	else
		myproxy = m_upload->m_proxy;
	
	comboProxy->addItem(tr("None", "No proxy"));
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].toString());
		comboProxy->setItemData(i+1, listProxy[i].uuid.toString());
		
		if(listProxy[i].uuid == myproxy)
			comboProxy->setCurrentIndex(i+1);
	}
}

void RapidshareOptsWidget::browse(QLineEdit* target)
{
	QString _new = QFileDialog::getSaveFileName(getMainWindow(), "FatRat", target->text());
	if(!_new.isEmpty())
		target->setText(_new);
}

void RapidshareOptsWidget::browseDownload()
{
	browse(lineLinksDownload);
}

void RapidshareOptsWidget::browseKill()
{
	browse(lineLinksKill);
}

void RapidshareOptsWidget::accepted()
{
	QList<Transfer*> tl;
	QUuid proxy = comboProxy->itemData(comboProxy->currentIndex()).toString();
	QString user, pass, down, kill;
	RapidshareUpload::AccountType acc = RapidshareUpload::AccountType( comboAccount->currentIndex() );
	
	user = lineUsername->text();
	pass = linePassword->text();
	down = lineLinksDownload->text();
	kill = lineLinksKill->text();
	
	if(m_multi)
		tl = *m_multi;
	else
		tl << m_upload;
	
	foreach(Transfer* t, tl)
	{
		bool bChanged = false;
		RapidshareUpload* tu = (RapidshareUpload*) t;
		
		if(user != tu->m_strUsername)
		{
			bChanged = true;
			tu->m_strUsername = user;
		}
		if(pass != tu->m_strPassword)
		{
			bChanged = true;
			tu->m_strPassword = pass;
		}
		if(acc != tu->m_type)
		{
			bChanged = true;
			tu->m_type = acc;
		}
		if(proxy != tu->m_proxy)
		{
			bChanged = true;
			tu->m_proxy = proxy;
		}
		tu->m_strLinksDownload = down;
		tu->m_strLinksKill = kill;
		
		if(bChanged)
		{
			tu->m_nDone = 0;
			tu->m_nFileID = -1;
			tu->m_strKillID.clear();
			
			if(tu->isActive())
			{
				tu->changeActive(false);
				tu->changeActive(true);
			}
		}
	}
}

void RapidshareOptsWidget::accTypeChanged(int now)
{
	lineUsername->setEnabled(now != 0);
	linePassword->setEnabled(now != 0);
}

bool RapidshareOptsWidget::accept()
{
	if(!comboAccount->currentIndex())
		return true;
	else
		return !lineUsername->text().isEmpty() && !linePassword->text().isEmpty();
}

///////////////////////////////////////////////

RapidshareSettings::RapidshareSettings(QWidget* me, QObject* p)
	: QObject(p)
{
	setupUi(me);
	comboAccount->addItems( QStringList() << tr("No account") << tr("Collector's account") << tr("Premium account") );
	connect(comboAccount, SIGNAL(currentIndexChanged(int)), this, SLOT(accTypeChanged(int)));
	connect(toolDownload, SIGNAL(clicked()), this, SLOT(browseDownload()));
	connect(toolKill, SIGNAL(clicked()), this, SLOT(browseKill()));
}

void RapidshareSettings::accTypeChanged(int now)
{
	lineUsername->setEnabled(now != 0);
	linePassword->setEnabled(now != 0);
	checkStatus->setEnabled(now == 2);
}

void RapidshareSettings::load()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QString proxy;
	
	int acc = getSettingsValue("rapidshare/account").toInt();
	comboAccount->setCurrentIndex(acc);
	lineUsername->setText(getSettingsValue("rapidshare/username").toString());
	linePassword->setText(getSettingsValue("rapidshare/password").toString());
	lineLinksDownload->setText(getSettingsValue("rapidshare/down_links").toString());
	lineLinksKill->setText(getSettingsValue("rapidshare/kill_links").toString());
	checkStatus->setChecked(getSettingsValue("rapidshare/status_widget").toBool());
	
	accTypeChanged(acc);
	
	comboProxy->clear();
	comboProxy->addItem(tr("None", "No proxy"));
	
	proxy = getSettingsValue("rapidshare/proxy").toString();
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].toString());
		comboProxy->setItemData(i+1, listProxy[i].uuid.toString());
		
		if(listProxy[i].uuid == proxy)
			comboProxy->setCurrentIndex(i+1);
	}
}

void RapidshareSettings::accepted()
{
	int acctype = comboAccount->currentIndex();
	QString user, password;
	QList<Auth> auths = Auth::loadAuths();
	int ix = -1;
	const char* REGEXP = "http://rapidshare\\.com/files/\\d+/.+";
	
	user = lineUsername->text();
	password = linePassword->text();
	
	for(int i=0;i<auths.size();i++)
	{
		if(auths[i].strRegExp == REGEXP)
		{
			ix = i;
			break;
		}
	}
	
	if(!acctype)
	{
		if(ix >= 0)
			auths.removeAt(ix);
	}
	else
	{
		if(ix >= 0)
		{
			auths[ix].strUser = user;
			auths[ix].strPassword = password;
		}
		else
		{
			Auth auth = { REGEXP, user, password };
			auths << auth;
		}
	}
	
	Auth::saveAuths(auths);
	
	setSettingsValue("rapidshare/account", acctype);
	setSettingsValue("rapidshare/username", user);
	setSettingsValue("rapidshare/password", password);
	setSettingsValue("rapidshare/proxy", comboProxy->itemData(comboProxy->currentIndex()).toString());
	setSettingsValue("rapidshare/down_links", lineLinksDownload->text());
	setSettingsValue("rapidshare/kill_links", lineLinksKill->text());
	
	setSettingsValue("rapidshare/status_widget", checkStatus->isChecked());
	RapidshareUpload::applySettings();
}

void RapidshareSettings::browseDownload()
{
	RapidshareOptsWidget::browse(lineLinksDownload);
}

void RapidshareSettings::browseKill()
{
	RapidshareOptsWidget::browse(lineLinksKill);
}


bool RapidshareSettings::accept()
{
	if(!comboAccount->currentIndex())
		return true;
	else
		return !lineUsername->text().isEmpty() && !linePassword->text().isEmpty();
}


