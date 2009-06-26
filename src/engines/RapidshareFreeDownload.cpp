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

#include "RapidshareFreeDownload.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "Proxy.h"
#include "fatrat.h"
#include <QHttp>
#include <QBuffer>
#include <QRegExp>
#include <QApplication>
#include <QMessageBox>
#include <QMutex>

static QMutex m_mInstanceActive;

RapidshareFreeDownload::RapidshareFreeDownload()
	: m_http(0), m_buffer(0), m_nSecondsLeft(-1), m_bHasLock(false)
{
	m_proxy = getSettingsValue("rapidshare/proxy").toString();
}

RapidshareFreeDownload::~RapidshareFreeDownload()
{
	if(isActive())
		changeActive(false);
}

void RapidshareFreeDownload::init(QString source, QString target)
{
	m_strOriginal = source;
	m_strTarget = target;
	deriveName();
	
	if(QThread::currentThread() != QApplication::instance()->thread())
		moveToThread(QApplication::instance()->thread());
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

QString RapidshareFreeDownload::name() const
{
	if(m_curl)
		return CurlDownload::name();
	else
		return m_strName;
}

void RapidshareFreeDownload::load(const QDomNode& map)
{
	m_strOriginal = getXMLProperty(map, "rapidshare_original");
	m_strTarget = getXMLProperty(map, "rapidshare_target");
	
	Transfer::load(map);
	deriveName();
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

void RapidshareFreeDownload::deriveName()
{
	int x = m_strOriginal.lastIndexOf('/');
	if(x < 0)
		m_strName = m_strOriginal;
	else
	{
		m_strName = m_strOriginal.mid(x+1);
		if(m_strName.endsWith(".html"))
			m_strName.resize(m_strName.size()-5);
	}
}

qulonglong RapidshareFreeDownload::done() const
{
	if(m_curl)
		return CurlDownload::done();
	else if(m_state == Completed)
		return m_nTotal;
	else
		return 0;
}

void RapidshareFreeDownload::setObject(QString newdir)
{
	if(m_curl)
		CurlDownload::setObject(newdir);
	m_strTarget = newdir;
}

void RapidshareFreeDownload::save(QDomDocument& doc, QDomNode& map) const
{
	if(m_state != Completed)
		Transfer::save(doc, map);
	else
		CurlDownload::save(doc, map);
	
	setXMLProperty(doc, map, "rapidshare_original", m_strOriginal);
	setXMLProperty(doc, map, "rapidshare_target", m_strTarget);
}

void RapidshareFreeDownload::changeActive(bool bActive)
{
	if(bActive)
	{
		if(!m_bHasLock && !m_mInstanceActive.tryLock())
		{
			enterLogMessage(m_strMessage = tr("You cannot have multiple RS.com FREE downloads."));
			setState(Failed);
			return;
		}
		
		m_bLongWaiting = false;
		
		QUrl url(m_strOriginal);
		m_http = new QHttp("rapidshare.com", 80, this);
		m_buffer = new QBuffer(m_http);
		
		m_http->setProxy(Proxy::getProxy(m_proxy));
		connect(m_http, SIGNAL(done(bool)), this, SLOT(firstPageDone(bool)));
		m_http->get(url.path(), m_buffer);
		
		m_strMessage = tr("Loading the first page");
		m_bHasLock = true;
		m_nTotal = 0;
	}
	else
	{
		if(m_bHasLock)
		{
			m_mInstanceActive.unlock();
			m_bHasLock = false;
		}
		
		if(m_curl)
			CurlDownload::changeActive(false);
		m_nSecondsLeft = -1;
		m_timer.stop();
	}
}

void RapidshareFreeDownload::secondElapsed()
{
	if(!isActive())
		return;
	
	if(--m_nSecondsLeft <= 0)
	{
		m_strMessage.clear();
		m_timer.stop();

		if(m_bLongWaiting)
		{
			// restart the procedure
			changeActive(true);
		}
		else
		{	
			try
			{
				m_nTotal = 0;
				
				CurlDownload::init(m_downloadUrl.toString(), m_strTarget);
				m_urls[0].proxy = m_proxy;
				QFile::remove(filePath());
				CurlDownload::changeActive(true);
			}
			catch(const RuntimeException& e)
			{
				m_strMessage = e.what();
				setState(Failed);
			}
		}
	}
	else
	{
		m_strMessage = tr("%1:%2 seconds left").arg(m_nSecondsLeft/60).arg(m_nSecondsLeft%60,2,10,QChar('0'));
	}
}

void RapidshareFreeDownload::firstPageDone(bool error)
{
	m_http->deleteLater();
	
	if(!isActive())
		return;
	
	try
	{
		if(error)
			throw tr("Failed to load the download's first page.");
		
		QRegExp re("form id=\"ff\" action=\"([^\"]+)");
		if(re.indexIn(m_buffer->data()) < 0)
			throw tr("Failed to parse the download's first page.");
		
		m_downloadUrl = re.cap(1);
		
		m_http = new QHttp(m_downloadUrl.host(), 80, this);
		m_buffer = new QBuffer(m_http);
		m_http->setProxy(Proxy::getProxy(m_proxy));
		
		connect(m_http, SIGNAL(done(bool)), this, SLOT(secondPageDone(bool)));
		m_http->post(m_downloadUrl.path(), "dl.start=Free", m_buffer);
		
		m_strMessage = tr("Loading the second page");
	}
	catch(QString err)
	{
		enterLogMessage(m_strMessage = err);
		setState(Failed);
		m_http = 0;
		m_buffer = 0;
	}
}

void RapidshareFreeDownload::secondPageDone(bool error)
{
	m_http->deleteLater();
	
	if(!isActive())
		return;
	
	try
	{
		if(error)
			throw tr("Failed to load the download's waiting page.");
		
		QRegExp re("var c=(\\d+);");
		if(re.indexIn(m_buffer->data()) < 0)
		{
			re.setPattern("(\\d+) minutes");
			if(re.indexIn(m_buffer->data()) < 0)
				m_nSecondsLeft = 30;
			else
				m_nSecondsLeft = re.cap(1).toInt() * 60;
			m_bLongWaiting = true;
		}
		else
		{
			m_nSecondsLeft = re.cap(1).toInt();

			QRegExp re2("<form name=\"dlf\" action=\"([^\"]+)");
			if(re2.indexIn(m_buffer->data()) < 0)
				throw tr("Failed to parse the download's waiting page.");

			m_downloadUrl = re2.cap(1);
		}
		m_timer.start(1000);
	}
	catch(QString err)
	{
		m_strMessage = err;
		setState(Failed);
		m_http = 0;
		m_buffer = 0;
	}
}

int RapidshareFreeDownload::acceptable(QString uri, bool)
{
	QRegExp re("http://rapidshare\\.com/files/\\d+/.+");
	if(re.exactMatch(uri))
	{
		if(getSettingsValue("rapidshare/account").toInt() == 2)
			return 1; // user has an account -> let CurlDownload handle this
		else
			return 3; // no account, it's up to us
	}
	return 0;
}

void RapidshareFreeDownload::setState(State state)
{
	if(state == Transfer::Completed && done() < 10*1024)
	{
		QByteArray data;
		m_file.seek(0);
		data = m_file.readAll();
		
		if(data.indexOf("<h1>Error</h1>") != -1)
		{
			m_file.remove();
			setState(Failed);
			m_nTotal = 0;
			m_strMessage = tr("Failed to download the file.");
			return;
		}
	}
	
	Transfer::setState(state);
}

WidgetHostChild* RapidshareFreeDownload::createOptionsWidget(QWidget* w)
{
	return new RapidshareFreeDownloadOptsForm(w, this);
}

//////////////////////////////////////////////////////////////

RapidshareFreeDownloadOptsForm::RapidshareFreeDownloadOptsForm(QWidget* w, RapidshareFreeDownload* d)
	: m_download(d)
{
	setupUi(w);
}

void RapidshareFreeDownloadOptsForm::load()
{
	lineURL->setText(m_download->m_strOriginal);
}

bool RapidshareFreeDownloadOptsForm::accept()
{
	if(!RapidshareFreeDownload::acceptable(lineURL->text(), false))
	{
		QMessageBox::warning(getMainWindow(), "FatRat", QObject::tr("Invalid URL."));
		return false;
	}
	else
		return true;
}

void RapidshareFreeDownloadOptsForm::accepted()
{
	m_download->m_strOriginal = lineURL->text();
}

