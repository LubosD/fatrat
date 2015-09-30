/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "MetalinkDownload.h"
#include "MetalinkSettings.h"
#include "Settings.h"
#include "config.h"
#include <QUrl>
#include <QDomDocument>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTemporaryFile>

#include "RuntimeException.h"
#include "Queue.h"

#ifdef WITH_BITTORRENT
#	include "TorrentDownload.h"
#endif
#ifdef WITH_CURL
#	include "CurlDownload.h"
#endif

MetalinkDownload::MetalinkDownload()
	: m_network(0), m_file(0), m_reply(0)
{

}

MetalinkDownload::~MetalinkDownload()
{

}

int MetalinkDownload::acceptable(QString url, bool)
{
	QUrl uurl(url);
	if (uurl.path().endsWith(".metalink") || uurl.path().endsWith(".meta4"))
		return 3;
	return 1;
}

void MetalinkDownload::globalInit()
{
	SettingsItem si;

	si.icon = DelayedIcon(":/fatrat/metalink.png");
	si.title = tr("Metalink");
	si.lpfnCreate = MetalinkSettings::create;

	addSettingsPage(si);
}

void MetalinkDownload::changeActive(bool nowActive)
{
	if (nowActive)
	{
		if (!m_strSource.startsWith('/'))
		{
			m_file = new QTemporaryFile(this);
			if (!m_file->open())
			{
				m_strMessage = tr("Failed to create a temporary file");
				setState(Failed);
				return;
			}

			m_network = new QNetworkAccessManager(this);
			connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(done(QNetworkReply*)));

			m_reply = m_network->get(QNetworkRequest(m_strSource));
			connect(m_reply, SIGNAL(readyRead()), this, SLOT(networkReadyRead()));
		}
		else
			QMetaObject::invokeMethod(this, "processMetalink", Qt::QueuedConnection, Q_ARG(QString, m_strSource));
	}
}

void MetalinkDownload::networkReadyRead()
{
	m_file->write(m_reply->readAll());
}

void MetalinkDownload::done(QNetworkReply* reply)
{
	networkReadyRead();

	reply->deleteLater();
	m_reply = 0;

	if (reply->error() != QNetworkReply::NoError)
	{
		m_strMessage = tr("Failed to download the metalink file");
		setState(Failed);
		return;
	}

	m_file->flush();
	processMetalink(m_file->fileName());
	//delete m_file;
}

void MetalinkDownload::init(QString source, QString target)
{
	m_strTarget = target;
	m_strSource = source;
}

QString MetalinkDownload::name() const
{
	//if (!isActive())
	{
		QString path = QUrl(m_strSource).path();
		int p = path.lastIndexOf('/');
		if (p < 0)
			return path;
		else
			return path.mid(p+1);
	}
}

void MetalinkDownload::load(const QDomNode& map)
{
	m_strSource = getXMLProperty(map, "source");
	m_strTarget = getXMLProperty(map, "target");

	Transfer::load(map);
}

void MetalinkDownload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);

	setXMLProperty(doc, map, "source", m_strSource);
	setXMLProperty(doc, map, "target", m_strTarget);
}

void MetalinkDownload::processMetalink(QString fileName)
{
	QFile file(fileName);
	QDomDocument doc;

	if (!file.open(QIODevice::ReadOnly))
	{
		m_strMessage = tr("Failed to read the metalink file: %1").arg(file.errorString());
		setState(Failed);
		return;
	}

	QString msg;
	if (!doc.setContent(&file, false, &msg))
	{
		m_strMessage = tr("Failed to read the metalink file: %1").arg(msg);
		setState(Failed);
		return;
	}

	QList<MetaFile> files;
	QDomElement root = doc.documentElement();
	QDomElement dfile = root.firstChildElement("file");
	while (!dfile.isNull())
	{
		QDomNode snode = dfile.firstChild();
		MetaFile metaFile;
		metaFile.name = dfile.attribute("name");
		metaFile.fileSize = 0;

		metaFile.hasHTTP = metaFile.hasTorrent = false;

		while (!snode.isNull())
		{
			if (!snode.isElement())
			{
				snode = snode.nextSibling();
				continue;
			}
			QDomElement elem = snode.toElement();
			QString tagName = elem.tagName();

			if (tagName == "url")
			{
				QString url = elem.text();
				int priority = 5;

				if (elem.hasAttribute("priority"))
					priority = elem.attribute("priority").toInt();
				if (elem.hasAttribute("preference"))
					priority = -elem.attribute("preference").toInt();

				if (elem.attribute("type") == "bittorrent")
				{
					metaFile.urls << Link(url, priority, true);
					metaFile.hasTorrent = true;
				}
				else if (url.startsWith("http://") || url.startsWith("ftp://"))
				{
					metaFile.urls << Link(url, priority, false);
					metaFile.hasHTTP = true;
				}
			}
			else if (tagName == "metaurl" && elem.attribute("mediatype") == "torrent")
			{
				int priority = elem.attribute("priority", "5").toInt();
				metaFile.urls << Link(elem.text(), priority, true);
				metaFile.hasTorrent = true;
			}
			else if (tagName == "description")
				metaFile.comment = elem.text();
			else if (tagName == "size")
				metaFile.fileSize = elem.text().toLongLong();
			else if (tagName == "hash")
			{
				QString htype = elem.attribute("type");
				//if (htype == "sha-256")
				//	hashSHA256 = elem.text();
				//else
				if (htype == "sha-1")
					metaFile.hashSHA1 = elem.text();
				else if (htype == "md5")
					metaFile.hashMD5 = elem.text();
			}
			else if (tagName == "verification")
			{
				QDomElement hash = elem.firstChildElement("hash");
				while (!hash.isNull())
				{
					QString htype = hash.attribute("type");
					if (htype == "md5")
						metaFile.hashMD5 = hash.text();
					else if (htype == "sha1")
						metaFile.hashSHA1 = hash.text();
					hash = hash.nextSiblingElement("hash");
				}
			}

			snode = snode.nextSibling();
		}
		dfile = dfile.nextSiblingElement("file");

		files << metaFile;
	}

	if (files.isEmpty())
	{
		m_strMessage = tr("No files specified inside the metalink");
		setState(Failed);
		return;
	}

	for(int i=0;i<files.size();i++)
	{
		MetaFile& m = files[i];
		int mode = getSettingsValue("metalink/mode").toInt();

		qSort(m.urls);

		if (mode == 0 && !m.hasHTTP)
			mode = 1;
		if (mode == 1 && !m.hasTorrent)
			mode = 0;

#if !defined(WITH_CURL) && defined(WITH_BITTORRENT)
		if (mode == 0)
			mode = 1;
#endif
#if !defined(WITH_BITTORRENT) && defined(WITH_CURL)
		if (mode == 1)
			mode = 0;
#endif

decide:
#ifdef WITH_BITTORRENT
		if (mode == 1)
		{
			TorrentDownload* t = 0;
			// just find the first BitTorrent link
			for (int j=0;j<m.urls.size();j++)
			{
				if (m.urls[j].isTorrent)
				{
					t = new TorrentDownload;
					try
					{
						t->init(m.urls[j].url, m_strTarget);
						if (!i)
							this->replaceItself(t);
						else
							this->myQueue()->add(t);
						if (isActive())
							t->setState(Waiting);
						this->deleteLater();
					}
					catch (const RuntimeException& e)
					{
						m_strMessage = e.what();
						setState(Failed);
						delete t;
						return;
					}

					break;
				}
			}
			for (int j=0;j<m.urls.size();j++)
			{
				if (m.urls[j].isTorrent)
					continue;
				t->addUrlSeed(m.urls[j].url);
			}

#ifndef WITH_CURL
			if (!t)
			{
				m_strMessage = tr("No torrent link found for %1").arg(m.name);
				setState(Failed);
				return;
			}
#endif
		}
#endif
#ifdef WITH_CURL
		// gather all HTTP/FTP links
		if (mode == 0)
		{
			CurlDownload* t = 0;
			for (int j=0;j<m.urls.size();j++)
			{
				if (!m.urls[j].isTorrent)
				{
					if (!t)
					{
						t = new CurlDownload;
						try
						{
							t->init(m.urls[j].url, m_strTarget);
						}
						catch (const RuntimeException& e)
						{
							m_strMessage = e.what();
							setState(Failed);
							return;
						}
					}
					else
					{
						UrlClient::UrlObject obj;
						obj.url = m.urls[j].url;
						t->m_urls << obj;
					}
				}
			}

			if (t)
			{
				if (!i)
					this->replaceItself(t);
				else
					this->myQueue()->add(t);
				if (isActive())
					t->setState(Waiting);
				this->deleteLater();
			}
			else
			{
#ifdef WITH_BITTORRENT
				mode = 1;
				goto decide;
#else
				m_strMessage = tr("No HTTP/FTP link found for %1").arg(m.name);
				setState(Failed);
#endif
			}
		}
#endif
	}


}

QString MetalinkDownload::remoteURI() const
{
	return m_strSource;
}
