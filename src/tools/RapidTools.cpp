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
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>

RapidTools::RapidTools()
	: m_httpRShare(0), m_httpRF(0)
{
	setupUi(this);
	
	connect(pushCheck, SIGNAL(clicked()), this, SLOT(checkRShareLinks()));
	connect(pushDownload, SIGNAL(clicked()), this, SLOT(downloadRShareLinks()));
	connect(pushReset, SIGNAL(clicked()), this, SLOT(reserRShare()));
	
	connect(pushExtract, SIGNAL(clicked()), this, SLOT(extractRFLinks()));
	connect(pushDownloadFolder, SIGNAL(clicked()), this, SLOT(downloadRFLinks()));
	
	checkIgnoreInvalid->setChecked(getSettingsValue("rstools/ignore_invalid").toBool());

	m_httpRShare = new QNetworkAccessManager(this);
	connect(m_httpRShare, SIGNAL(finished(QNetworkReply*)), this, SLOT(doneRShare(QNetworkReply*)));
	m_httpRF = new QNetworkAccessManager(this);
	connect(m_httpRF, SIGNAL(finished(QNetworkReply*)), this, SLOT(doneRF(QNetworkReply*)));
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
		QRegExp re("http://rapidshare.com/files/(\\d+)/(.+)");
		QString ids;
		QString names;
		bool skipInvalid = checkIgnoreInvalid->isChecked();
		
		while(names.size() < 29000 && ids.size() < 2900 && !m_listRSharePending.isEmpty())
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
			
			if (!ids.isEmpty())
				ids += ',';
			ids += re.cap(1);
			if (!names.isEmpty())
				names += ',';
			names += re.cap(2);
		}
		
		m_httpRShare->get(QNetworkRequest(
				QString("http://api.rapidshare.com/cgi-bin/rsapi.cgi?sub=checkfiles_v1&files=%1&filenames=%2")
				.arg(ids).arg(names)));
		
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

void RapidTools::doneRShare(QNetworkReply* reply)
{
	QString result;
	reply->deleteLater();

	if(reply->error() != QNetworkReply::NoError)
	{
		QMessageBox::critical(this, "FatRat", tr("Server failed to process our query."));
		return;
	}

	while(!reply->atEnd())
	{
		QString line = reply->readLine();
		QStringList list = line.split(',');
		if (list.size() < 5)
			continue;
		QString reconstructed = QString("http://rapidshare.com/files/%1/%2").arg(list[0]).arg(list[1]);
		int status = list[4].toInt();
		const char* color;

		if (status == 1 || status >= 50)
		{
			color = "green";
			m_strRShareWorking += reconstructed + '\n';
		}
		else
			color = "red";
		result += QString("<font color=%1>%2</font><br>").arg(color).arg(reconstructed);
	}
	
	if(!result.isEmpty())
		textLinks->append(result);
	m_mapRShare.clear();
	
	if(m_listRSharePending.isEmpty())
		pushCheck->setEnabled(true);
	else
		doRShareCheck();
}

void RapidTools::extractRFLinks()
{
	QString url = lineURL->text().trimmed();
	QRegExp reUrl("http://rapidshare.com/users/([^/]+).*");
	
	if(!reUrl.exactMatch(url))
	{
		QMessageBox::warning(this, "FatRat", tr("An invalid link has been encountered: %1").arg(url));
	}
	else if(!url.isEmpty())
	{
		textFolderContents->clear();
		pushExtract->setDisabled(true);
		
		m_httpRF->get(QNetworkRequest(
				QString("http://api.rapidshare.com/cgi-bin/rsapi.cgi?sub=viewlinklist_v1&linklist=")+reUrl.cap(1)));
	}
}

void RapidTools::doneRF(QNetworkReply* reply)
{
	reply->deleteLater();

	if(reply->error() != QNetworkReply::NoError)
	{
		QMessageBox::critical(this, "FatRat", tr("Server failed to process our query."));
		return;
	}

	QRegExp re("\"([^\"]+)\",\"([^\"]+)\",\"([^\"]+)\",\"([^\"]+)\".*");
	while (!reply->atEnd())
	{
		QString line = reply->readLine();
		if (!re.exactMatch(line))
			continue;
		QString reconstructed = QString("http://rapidshare.com/files/%1/%2").arg(re.cap(3)).arg(re.cap(4));
		textFolderContents->append(reconstructed);

	}
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

