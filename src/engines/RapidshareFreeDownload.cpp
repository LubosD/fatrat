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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegExp>
#include <QApplication>
#include <QMessageBox>
#include <QMutex>

static QMutex m_mInstanceActive;

RapidshareFreeDownload::RapidshareFreeDownload()
	: m_nSecondsLeft(-1), m_bHasLock(false)
{
	m_proxy = getSettingsValue("rapidshare/proxy").toString();
	m_network = new QNetworkAccessManager(this);
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(httpFinished(QNetworkReply*)));
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
	
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

QString RapidshareFreeDownload::name() const
{
	if(isActive())
		return CurlDownload::name();
	else
		return m_strFile;
}

void RapidshareFreeDownload::load(const QDomNode& map)
{
	m_strOriginal = getXMLProperty(map, "rapidshare_original");
	m_strTarget = getXMLProperty(map, "rapidshare_target");
	m_nTotal = getXMLProperty(map, "knowntotal").toLongLong();
	
	Transfer::load(map);
	deriveName();
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
}

void RapidshareFreeDownload::deriveName()
{
	int x = m_strOriginal.lastIndexOf('/');
	if(x < 0)
		m_strFile = m_strOriginal;
	else
	{
		m_strFile = m_strOriginal.mid(x+1);
		if(m_strFile.endsWith(".html"))
			m_strFile.resize(m_strFile.size()-5);
	}
}

qulonglong RapidshareFreeDownload::done() const
{
	if(isActive())
		return CurlDownload::done();
	else if(m_state == Completed)
		return m_nTotal;
	else
		return 0;
}

void RapidshareFreeDownload::setObject(QString newdir)
{
	if(isActive())
		CurlDownload::setObject(newdir);
	m_strTarget = newdir;
}

void RapidshareFreeDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "rapidshare_original", m_strOriginal);
	setXMLProperty(doc, map, "rapidshare_target", m_strTarget);
	setXMLProperty(doc, map, "knowntotal", QString::number(m_nTotal));
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

		QRegExp re("http://(www\\.)?rapidshare\\.com/files/(\\d+)/(.+)");
		if (!re.exactMatch(m_strOriginal))
		{
			m_strMessage = tr("The URL is invalid");
			setState(Failed);
			return;
		}

		QNetworkRequest request;
		request.setUrl(QString("http://api.rapidshare.com/cgi-bin/rsapi.cgi?sub=download_v1&fileid=%1&filename=%2").arg(m_nFileID = re.cap(2).toLongLong()).arg(re.cap(3)));
		m_network->setProxy(Proxy::getProxy(m_proxy));
		
		m_network->get(request);
		
		m_strMessage = tr("Calling the API");
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
		
		CurlDownload::changeActive(false);
		m_nSecondsLeft = -1;
		m_timer.stop();
	}
}

void RapidshareFreeDownload::httpFinished(QNetworkReply* reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		m_strMessage = reply->errorString();
		setState(Failed);
		return;
	}

	QByteArray response = reply->readAll();
	if (response.startsWith("ERROR"))
	{
		QRegExp re("(\\d+) sec");
		if (re.indexIn(response) != -1)
		{
			m_bLongWaiting = true;
			m_nSecondsLeft = re.cap(1).toInt();
			m_timer.start(1000);
		}
		else
		{
			int nl = response.indexOf('\n');
			if (nl == -1)
				m_strMessage = response;
			else
				m_strMessage = response.left(nl);
			setState(Failed);
		}
	}
	else if (response.startsWith("DL:"))
	{
		QList<QByteArray> parts = response.split(',');

		if (parts.size() < 3)
		{
			m_strMessage = tr("Unknown server response");
			setState(Failed);
			return;
		}

		QByteArray hostname = parts[0].mid(3);
		QByteArray& dlauth = parts[1];

		m_downloadUrl = QString("http://%1/cgi-bin/rsapi.cgi?sub=download_v1&editparentlocation=0&bin=1&fileid=%2&filename=%3&dlauth=%4").arg(QString(hostname)).arg(m_nFileID).arg(m_strFile).arg(QString(dlauth));

		m_nSecondsLeft = parts[2].toInt();
		m_timer.start(1000);
	}
	else
	{
		m_strMessage = tr("Unknown server response");
		setState(Failed);
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

int RapidshareFreeDownload::acceptable(QString uri, bool)
{
	QRegExp re("http://(www\\.)?rapidshare\\.com/files/\\d+/.+");
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
		QFile file(filePath());
		if (file.open(QIODevice::ReadOnly))
		{
			if (file.readAll().contains("Error"))
			{
				file.remove();
				setState(Failed);
				m_nTotal = 0;
				m_strMessage = tr("Failed to download the file.");
				return;
			}
		}
	}
	
	Transfer::setState(state);
}

WidgetHostChild* RapidshareFreeDownload::createOptionsWidget(QWidget* w)
{
	return new RapidshareFreeDownloadOptsForm(w, this);
}

QString RapidshareFreeDownload::remoteURI() const
{
	return m_strOriginal;
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

