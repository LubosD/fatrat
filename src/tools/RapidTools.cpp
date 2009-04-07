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

#include "RapidTools.h"
#include "MainWindow.h"
#include "fatrat.h"
#include "Settings.h"
#include <QUrl>
#include <QMessageBox>
#include <QtDebug>

RapidTools::RapidTools()
	: m_httpRShare(0), m_httpRSafe(0), m_httpRF(0)
{
	setupUi(this);
	
	connect(pushCheck, SIGNAL(clicked()), this, SLOT(checkRShareLinks()));
	connect(pushDownload, SIGNAL(clicked()), this, SLOT(downloadRShareLinks()));
	connect(pushReset, SIGNAL(clicked()), this, SLOT(reserRShare()));
	
	connect(pushDecode, SIGNAL(clicked()), this, SLOT(decodeRSafeLinks()));
	connect(pushDownload2, SIGNAL(clicked()), this, SLOT(downloadRSafeLinks()));
	
	connect(pushExtract, SIGNAL(clicked()), this, SLOT(extractRFLinks()));
	connect(pushDownloadFolder, SIGNAL(clicked()), this, SLOT(downloadRFLinks()));
	
	checkIgnoreInvalid->setChecked(getSettingsValue("rstools/ignore_invalid").toBool());
}

RapidTools::~RapidTools()
{
	setSettingsValue("rstools/ignore_invalid", checkIgnoreInvalid->isChecked());
}

void RapidTools::checkRShareLinks()
{
	m_strRShareWorking.clear();
	m_listRSharePending = textLinks->toPlainText().split('\n', QString::SkipEmptyParts);
	
	if(!m_listRSharePending.isEmpty())
	{
		if(doRShareCheck())
			textLinks->clear();
	}
}

bool RapidTools::doRShareCheck()
{
	if(!m_listRSharePending.isEmpty())
	{
		QRegExp re("http://rapidshare.com/files/(\\d+)/");
		QByteArray data = "urls=";
		bool skipInvalid = checkIgnoreInvalid->isChecked();
		
		while(data.size() < 9*1024 && !m_listRSharePending.isEmpty())
		{
			QString link = m_listRSharePending.takeFirst().trimmed();
			if(re.indexIn(link) < 0)
			{
				if(!skipInvalid)
				{
					QMessageBox::warning(this, "FatRat", tr("An invalid link has been encountered: %1").arg(link));
					return false;
				}
				else
					continue;
			}
			
			m_mapRShare[re.cap(1).toULong()] = link;
			
			data += QUrl::toPercentEncoding(link);
			data += "%0D%0A";
		}
		
		m_httpRShare = new QHttp("rapidshare.com", 80, this);
		m_bufRShare = new QBuffer(m_httpRShare);
		connect(m_httpRShare, SIGNAL(done(bool)), this, SLOT(doneRShare(bool)));
		m_httpRShare->post("/cgi-bin/checkfiles.cgi", data, m_bufRShare);
		
		pushCheck->setDisabled(true);
	}
	return true;
}

void RapidTools::reserRShare()
{
	textLinks->setText(QString());
	m_strRShareWorking.clear();
}

void RapidTools::downloadRShareLinks()
{
	if(!m_strRShareWorking.isEmpty())
	{
		MainWindow* wnd = (MainWindow*) getMainWindow();
		wnd->addTransfer(m_strRShareWorking);
	}
}

void RapidTools::doneRShare(bool error)
{
	QString result;
	if(error)
	{
		QMessageBox::critical(this, "FatRat", tr("Server failed to process our query."));
	}
	else
	{
		QRegExp re("<a href=\"http://rapidshare.com/files/(\\d+)/[^\"]+\" target=\"_blank\">File to load</a>");
		
		const QByteArray& data = m_bufRShare->data();
		int pos = 0;
		
		while(true)
		{
			pos = re.indexIn(data, pos);
			
			if(pos < 0)
				break;
			
			unsigned long id = re.cap(1).toULong();
			QString url = m_mapRShare[id];
			m_mapRShare.remove(id);
			
			result += QString("<font color=green>%1</font><br>").arg(url);
			m_strRShareWorking += url + '\n';
			
			pos++;
		}
	}
	
	m_httpRShare->deleteLater();
	
	m_bufRShare = 0;
	m_httpRShare = 0;
	
	for(QMap<unsigned long,QString>::const_iterator it = m_mapRShare.constBegin();
		   it != m_mapRShare.constEnd(); it++)
	{
		result += QString("<font color=red>%1</font><br>").arg(it.value());
	}
	
	if(!result.isEmpty())
		textLinks->append(result);
	m_mapRShare.clear();
	
	if(m_listRSharePending.isEmpty())
		pushCheck->setEnabled(true);
	else if(!error)
		doRShareCheck();
}

void RapidTools::decodeRSafeLinks()
{
	QStringList list = textLinksSrc->toPlainText().split('\n', QString::SkipEmptyParts);
	
	foreach(QString str, list)
	{
		if(str.startsWith("http://www.rapidsave.net/"))
			str[18] = 'f';
		else if(str.startsWith("http://rapidsave.net/"))
			str[14] = 'f';
		else if(!str.startsWith("http://www.rapidsafe.net/"))
		{
			QMessageBox::warning(this, "FatRat", tr("An invalid link has been encountered: %1").arg(str));
			return;
		}
	}
	
	textLinksDst->clear();
	m_listRSafeSrc.clear();
	
	m_httpRSafe = new QHttp("www.rapidsafe.net", 80, this);
	
	pushDecode->setDisabled(true);
	
	foreach(QString str, list)
	{
		QUrl url(str);
		int r;
		
		QBuffer* buf = new QBuffer(m_httpRSafe);
		connect(m_httpRSafe, SIGNAL(requestFinished(int,bool)), this, SLOT(doneRSafe(int,bool)));
		connect(m_httpRSafe, SIGNAL(done(bool)), this, SLOT(doneRSafe(bool)));
		r = m_httpRSafe->get(url.path(), buf);
		
		m_listRSafeSrc[r] = str;
		m_mapRSafeBufs[r] = buf;
	}
}

void RapidTools::downloadRSafeLinks()
{
	QString text = textLinksDst->toPlainText();
	if(!text.isEmpty())
	{
		MainWindow* wnd = (MainWindow*) getMainWindow();
		wnd->addTransfer(text);
	}
}

void RapidTools::doneRSafe(int r, bool error)
{
	QBuffer* buffer = m_mapRSafeBufs[r];
	if(!buffer)
		return;
	
	if(!error)
	{
		const QByteArray& data = buffer->data();
		int pos;
		QString result = "http://rapidshare.com";
		
		pos = data.indexOf("<FORM ACTION=\"http://rapidshare.com");
		if(pos >= 0)
		{
			pos += 35;
			
			while(data[pos] == '&')
			{
				int c = data.mid(pos+3, 2).toInt(0, 16);
				pos += 6;
				result += char(c);
			}
			
			textLinksDst->append(result);
			m_listRSafeSrc.remove(r);
		}
		else
			error = true;
	}
	
	if(error)
	{
		QMessageBox::critical(this, "FatRat", tr("Server failed to process our query."));
	}
	
	m_mapRSafeBufs.remove(r);
	delete buffer;
}

void RapidTools::doneRSafe(bool)
{
	m_httpRSafe->deleteLater();
	pushDecode->setEnabled(true);
	m_httpRSafe = 0;
}

void RapidTools::extractRFLinks()
{
	QString url = lineURL->text();
	
	if(!url.startsWith("http://rapidshare.com/users/"))
	{
		QMessageBox::warning(this, "FatRat", tr("An invalid link has been encountered: %1").arg(url));
	}
	else if(!url.isEmpty())
	{
		textFolderContents->clear();
		pushExtract->setDisabled(true);
		
		m_httpRF = new QHttp("rapidshare.com", 80, this);
		m_bufRF = new QBuffer(m_httpRF);
		connect(m_httpRF, SIGNAL(done(bool)), this, SLOT(doneRF(bool)));
		m_httpRF->get(QUrl(url).path(), m_bufRF);
	}
}

void RapidTools::doneRF(bool error)
{
	if(!error)
	{
		const QByteArray& data = m_bufRF->data();
		QRegExp re("<a href=\"http://rapidshare.com/files/([^\"]+)");
		int pos = 0;
		
		while(true)
		{
			pos = re.indexIn(data, pos);
			if(pos < 0)
				break;
			
			textFolderContents->append( QString("http://rapidshare.com/files/") + re.cap(1));
			
			pos++;
		}
	}
	else
		QMessageBox::critical(this, "FatRat", tr("Server failed to process our query."));
	
	m_httpRF->deleteLater();
	m_httpRF = 0;
	m_bufRF = 0;
	pushExtract->setEnabled(true);
}

void RapidTools::downloadRFLinks()
{
	QString text = textFolderContents->toPlainText();
	if(!text.isEmpty())
	{
		MainWindow* wnd = (MainWindow*) getMainWindow();
		wnd->addTransfer(text);
	}
}

