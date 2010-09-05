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
#include "HttpDetails.h"
#include <errno.h>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <QMessageBox>
#include <QMenu>
#include <QColor>
#include <QtDebug>
#include <iostream>
#include <errno.h>

static const QColor g_colors[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::darkRed,
	Qt::darkGreen, Qt::darkBlue, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow };

CurlDownload::CurlDownload()
	: m_curl(0), m_nTotal(0), m_nStart(0), m_bAutoName(false), m_nameChanger(0), m_master(0)
{
	m_errorBuffer[0] = 0;
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateSegmentProgress()));
}

CurlDownload::~CurlDownload()
{
	if(isActive())
		changeActive(false);
}

void CurlDownload::init(QString uri, QString dest)
{
	UrlClient::UrlObject obj;
	
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
	obj.ftpMode = UrlClient::FtpPassive;
	
	m_dir = dest;
	m_dir.mkpath(".");
	
	QString scheme = obj.url.scheme();
	if(scheme != "http" && scheme != "ftp" && scheme != "sftp" && scheme != "https")
		throw RuntimeException(tr("Unsupported protocol: \"%1\"").arg(scheme));
	
	m_urls.clear();
	m_urls << obj;
	m_listActiveSegments << 0;
	
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

int anti_crash_fun() { return 0; }

void CurlDownload::changeActive(bool bActive)
{
	qDebug() << this << "changeActive" << bActive;
	if(bActive)
	{
		const qulonglong d = done();
		m_strMessage.clear();

		if(m_urls.isEmpty())
		{
			enterLogMessage(m_strMessage = tr("No URLs have been given"));
			setState(Failed);
			return;
		}

		autoCreateSegment();

		m_nameChanger = 0;

		QWriteLocker l(&m_segmentsLock);
		int active = 0;

		simplifySegments(m_segments);

		if(m_segments.size() == 1 && m_nTotal == d && d)
		{
			setState(Completed);
			return;
		}

		m_master = new CurlPollingMaster;
		CurlPoller::instance()->addTransfer(m_master);
		m_master->setMaxDown(m_nDownLimitInt);

		qDebug() << "The limit is" << m_nDownLimitInt;

		fixActiveSegmentsList();

		// 1) find free spots
		QList<FreeSegment> freeSegs;
		qlonglong lastEnd = 0;
		for(int i=0;i<m_segments.size();i++)
		{
			if (m_segments[i].offset > lastEnd)
				freeSegs << FreeSegment(lastEnd, m_segments[i].offset-lastEnd);
			lastEnd = m_segments[i].offset + m_segments[i].bytes;
		}
		if (lastEnd < m_nTotal)
			freeSegs << FreeSegment(lastEnd, m_nTotal - lastEnd);
		if (!m_nTotal && freeSegs.isEmpty())
			freeSegs << FreeSegment(lastEnd, -1);
		// 2) sort them
		qSort(freeSegs.begin(), freeSegs.end());

		// 3) make enough free spots
		qDebug() << "Found" << freeSegs.size() << "free spots";
		while(freeSegs.size() < m_listActiveSegments.size() && m_nTotal)
		{
			int pos = freeSegs.size()-1;

			// 4) split the largest segment into halves
			int odd = freeSegs[pos].bytes % 2;
			qlonglong half = freeSegs[pos].bytes / 2;
			if (half <= getSettingsValue("httpftp/minsegsize").toInt())
				break;

			freeSegs[pos].bytes = half + odd;
			freeSegs << FreeSegment(freeSegs[pos].offset + freeSegs[pos].bytes, freeSegs[pos].bytes - odd);

			qSort(freeSegs.begin(), freeSegs.end());
		}
		qDebug() << freeSegs.size() << "free spots after splitting work";

		// 5) now allot the free spots to active segments

		if (freeSegs.size() < m_listActiveSegments.size() && m_nTotal)
		{
			// the free spots were too small, remove some active segments
			QSet<int> set = m_listActiveSegments.toSet();
			QList<int> superficial;
			// first select segments that download from the same URL more than once altogether
			while (set.size() < m_listActiveSegments.size() && m_listActiveSegments.size() > freeSegs.size())
			{
				superficial = m_listActiveSegments;
				foreach (int u, set)
					superficial.removeOne(u);
				// making the superficials unique to do the removal evenly
				QSet<int> xset = superficial.toSet();
				for (QSet<int>::iterator it=xset.begin(); it != xset.end() && m_listActiveSegments.size() > freeSegs.size(); it++)
				{
					m_listActiveSegments.removeOne(*it);
				}
				set = m_listActiveSegments.toSet();
			}

			while (m_listActiveSegments.size() > freeSegs.size())
			{
				// the last chance is to pick randomly any segments
				// mathematically wrong - uneven distribution, but who cares
				int r = qrand() % m_listActiveSegments.size();
				m_listActiveSegments.removeAt(r);
			}
		}

		for(int i=0, j=freeSegs.size()-1;i<m_listActiveSegments.size();i++,j--)
		{
			// 6) create a written segment for every active segment
			Segment seg;
			seg.offset = freeSegs[j].offset;
			seg.bytes = 0;
			seg.color = allocateSegmentColor();
			seg.urlIndex = m_listActiveSegments[i];

			// 7) now let's start a download thread for that segment
			startSegment(seg, freeSegs[j].bytes);

			m_segments << seg;

			// allow only one segment if we don't know the file size yet
			if(!m_nTotal)
				break;
		}

		// 8) update the segment progress every 500 miliseconds
		m_timer.start(500);
	}
	else if(m_master != 0)
	{
		updateSegmentProgress();

		m_segmentsLock.lockForWrite();
		for(int i=0;i<m_segments.size();i++)
		{
			if(!m_segments[i].client)
				continue;
			m_master->removeTransfer(m_segments[i].client);
			m_segments[i].client->stop();
			//delete m_segments[i].client;
			m_segments[i].client = 0;
			m_segments[i].color = Qt::black;
		}
		qDebug() << "Before final simplify segments:" << m_segments;
		simplifySegments(m_segments);
		qDebug() << "After final simplify segments:" << m_segments;
		m_segmentsLock.unlock();
		m_nameChanger = 0;
		m_timer.stop();

		CurlPoller::instance()->removeTransfer(m_master);
		//delete m_master;
		m_master = 0;
	}
}

void CurlDownload::startSegment(Segment& seg, qlonglong bytes)
{
	qDebug() << "CurlDownload::startSegment(): seg offset:" << seg.offset << "; bytes:" << bytes;

	if (!bytes)
		abort(); // this is a serious bug

	std::string spath = filePath().toStdString();
	int file = open(spath.c_str(), O_CREAT|O_RDWR|O_LARGEFILE, 0666);
	if(file < 0)
	{
		enterLogMessage(m_strMessage = strerror(errno));
		setState(Failed);
		return;
	}

	seg.client = new UrlClient;
	seg.client->setRange(seg.offset, (bytes > 0) ? seg.offset+bytes : -1);
	seg.client->setSourceObject(m_urls[seg.urlIndex]);
	seg.client->setTargetObject(file);

	connect(seg.client, SIGNAL(renameTo(QString)), this, SLOT(clientRenameTo(QString)));
	connect(seg.client, SIGNAL(logMessage(QString)), this, SLOT(clientLogMessage(QString)));
	connect(seg.client, SIGNAL(done(QString)), this, SLOT(clientDone(QString)));
	connect(seg.client, SIGNAL(failure(QString)), this, SLOT(clientFailure(QString)));
	connect(seg.client, SIGNAL(totalSizeKnown(qlonglong)), this, SLOT(clientTotalSizeKnown(qlonglong)));

	seg.client->setPollingMaster(m_master);
	seg.client->start();
	m_master->addTransfer(static_cast<CurlUser*>(seg.client));
}

bool CurlDownload::Segment::operator<(const Segment& s2) const
{
	return this->offset < s2.offset;
}

bool CurlDownload::FreeSegment::operator <(const FreeSegment& s2) const
{
	return this->bytes < s2.bytes;
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
	down = up = 0;
	if(m_master != 0)
		m_master->speeds(down, up);
}

qulonglong CurlDownload::total() const
{
	return m_nTotal;
}

qulonglong CurlDownload::done() const
{
	m_segmentsLock.lockForRead();
	qlonglong total = 0;

	for(int i=0;i<m_segments.size();i++)
		total += m_segments[i].bytes;

	m_segmentsLock.unlock();
	return total;
}

void CurlDownload::load(const QDomNode& map)
{
	m_dir = getXMLProperty(map, "dir");
	m_nTotal = getXMLProperty(map, "knowntotal").toULongLong();
	m_strFile = getXMLProperty(map, "filename");
	m_bAutoName = getXMLProperty(map, "autoname").toInt() != 0;

	QStringList activeSegments = getXMLProperty(map, "activesegments").split(',');
	m_listActiveSegments.clear();
	foreach(QString seg, activeSegments)
		m_listActiveSegments << seg.toInt();
	
	QDomElement url = map.firstChildElement("url");
	while(!url.isNull())
	{
		UrlClient::UrlObject obj;
		
		obj.url = getXMLProperty(url, "address");
		obj.strReferrer = getXMLProperty(url, "referrer");
		obj.proxy = getXMLProperty(url, "proxy");
		obj.ftpMode = (UrlClient::FtpMode) getXMLProperty(url, "ftpmode").toInt();
		obj.strBindAddress = getXMLProperty(url, "bindip");
		
		url = url.nextSiblingElement("url");
		
		m_urls << obj;
	}

	QDomElement segment, segments = map.firstChildElement("segments");
	
	m_segmentsLock.lockForWrite();
	if(!segments.isNull())
		segment = segments.firstChildElement("segment");
	while(!segment.isNull())
	{
		Segment data;

		data.offset = getXMLProperty(segment, "offset").toLongLong();
		data.bytes = getXMLProperty(segment, "bytes").toLongLong();
		//data.urlIndex = getXMLProperty(segment, "urlindex").toInt();
		data.urlIndex = -1;
		data.client = 0;

		segment = segment.nextSiblingElement("segment");
		m_segments << data;
	}

	if(m_strFile.isEmpty())
		generateName();

	autoCreateSegment();
	m_segmentsLock.unlock();
	
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

	QDomElement subSegments = doc.createElement("segments");

	m_segmentsLock.lockForRead();
	for(int i=0;i<m_segments.size();i++)
	{
		QDomElement sub = doc.createElement("segment");
		const Segment& s = m_segments[i];

		setXMLProperty(doc, sub, "offset", QString::number(s.offset));
		setXMLProperty(doc, sub, "bytes", QString::number(s.bytes));
		//setXMLProperty(doc, sub, "urlindex", QString::number(s.urlIndex));

		subSegments.appendChild(sub);
	}
	m_segmentsLock.unlock();
	map.appendChild(subSegments);

	QString activeSegments;
	foreach(int index, m_listActiveSegments)
	{
		if(!activeSegments.isEmpty())
			activeSegments += ',';
		activeSegments += index;
	}
	setXMLProperty(doc, map, "activesegments", activeSegments);
}

void CurlDownload::autoCreateSegment()
{
	QFileInfo fi(filePath());

	if(!fi.exists())
	{
		m_segments.clear();
		return;
	}

	if(m_segments.isEmpty())
	{
		Segment s;

		s.offset = s.bytes = 0;
		s.urlIndex = 0;
		s.client = 0;

		s.bytes = fi.size();
		if (s.bytes)
			m_segments << s;
	}
	else
	{
		// check for segments beyond the EOF (truncated file)
		qSort(m_segments);
		for (int i=0;i<m_segments.size();i++)
		{
			Segment& s = m_segments[i];
			if (s.offset >= fi.size())
				m_segments.removeAt(i--);
			else if (s.offset + s.bytes > fi.size())
				s.bytes = fi.size() - s.offset;
		}
	}
}

void CurlDownload::updateSegmentProgress()
{
	m_segmentsLock.lockForWrite();
	for(int i=0;i<m_segments.size();i++)
	{
		if(m_segments[i].client != 0)
			m_segments[i].bytes = m_segments[i].client->progress();
	}
	simplifySegments(m_segments);
	m_segmentsLock.unlock();
}

void CurlDownload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	//a = menu.addAction(tr("Switch mirror"));
	//connect(a, SIGNAL(triggered()), this, SLOT(switchMirror()));
	
	a = menu.addAction(tr("Compute hash..."));
	connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
}

/*
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
*/

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
	if(m_master != 0)
		m_master->setMaxDown(down);
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
	QUrl url = m_urls[0].url;
	url.setUserInfo(QString());
	return url.toString();
}


void CurlDownload::simplifySegments(QList<CurlDownload::Segment>& retval)
{
	qSort(retval);

	for(int i=1;i<retval.size();i++)
	{
		qlonglong pos = retval[i-1].offset+retval[i-1].bytes;
		if(retval[i].offset <= pos && !retval[i-1].client && !retval[i].client)
		{
			retval[i].bytes += retval[i].offset - retval[i-1].offset;
			retval[i].offset = retval[i-1].offset;
			retval.removeAt(--i);
		}
	}
}

void CurlDownload::fixActiveSegmentsList()
{
	for(int i = 0;i < m_listActiveSegments.size(); i++)
	{
		if (m_listActiveSegments[i] < 0 ||m_listActiveSegments[i] >= m_urls.size())
		{
			m_listActiveSegments.removeAt(i);
			i--;
		}
	}
	if(m_listActiveSegments.isEmpty())
		m_listActiveSegments << 0;
}

void CurlDownload::clientRenameTo(QString name)
{
	UrlClient* client = static_cast<UrlClient*>(sender());
	if(m_nameChanger && client != m_nameChanger)
		return;

	m_nameChanger = client;
	if(m_bAutoName)
		setTargetName(name);
}

void CurlDownload::clientLogMessage(QString msg)
{
	UrlClient* client = static_cast<UrlClient*>(sender());
	enterLogMessage(QString("0x%1 - %2").arg(long(client), 0, 16).arg(msg));
}

void CurlDownload::clientTotalSizeKnown(qlonglong bytes)
{
	qDebug() << "CurlDownload::clientTotalSizeKnown()" << bytes << "segs:" << m_listActiveSegments.size();
	if (!m_nTotal && m_listActiveSegments.size() > 1)
	{
		qDebug() << "Starting aditional segments";
		m_nTotal = bytes;
		// there are active segments we need to initialize now
		for(int i=1;i<m_listActiveSegments.size();i++)
			startSegment(m_listActiveSegments[i]);
	}
	else
		m_nTotal = bytes;
}

void CurlDownload::clientFailure(QString err)
{
	if (!isActive() || !m_master)
		return;

	qDebug() << "CurlDownload::clientFailure()" << err;
	m_strMessage = err;
	setState(Failed);
}

void CurlDownload::clientDone(QString error)
{
	if (!isActive() || !m_master)
		return;

	UrlClient* client = static_cast<UrlClient*>(sender());
	int urlIndex = 0;

	updateSegmentProgress();

	m_segmentsLock.lockForWrite();

	qDebug() << "---------- CurlDownload::clientDone():" << error << client;

	for(int i=0;i<m_segments.size();i++)
	{
		if(m_segments[i].client == client)
		{
			m_segments[i].bytes = client->progress();
			m_segments[i].client = 0;
			urlIndex = m_segments[i].urlIndex;
			m_segments[i].urlIndex = -1;
			m_segments[i].color = Qt::black;
		}
	}

	simplifySegments(m_segments);

	m_segmentsLock.unlock();

	m_master->removeTransfer(client);
	client->stop();
	//client->deleteLater();

	qulonglong d = done();
	if(d == total() && d)
	{
		setState(Completed);
	}
	else if(!error.isNull())
	{
		m_segmentsLock.lockForRead();
		bool allfailed = true;

		for(int i=0;i<m_segments.size();i++)
		{
			if(m_segments[i].client != 0)
			{
				allfailed = false;
				break;
			}
		}

		m_segmentsLock.unlock();

		if(allfailed)
		{
			setState(Failed);
			m_strMessage = error;
		}
		else
		{
			// TODO: show error
			// TODO: Replace segment?
		}
	}
	else
	{
		// The segment has been completed and the download is still incomplete
		// We need to find another free spot or split an allocated one
		//int down, up;
		//speeds(down, up);

		// Only if it has a meaning
		//if (total()-done() / down >= 30)
			startSegment(urlIndex);
	}
}

void CurlDownload::startSegment(int urlIndex)
{
	QWriteLocker l(&m_segmentsLock);
	qDebug() << "----------- CurlDownload::startSegment():" << urlIndex;

	// 1) find free spots, prefer unallocated free spots
	QList<FreeSegment> freeSegs, freeSegsUnallocated;
	qlonglong lastEnd = 0;
	for(int i=0;i<m_segments.size();i++)
	{
		if (m_segments[i].offset > lastEnd)
		{
			FreeSegment fs(lastEnd, m_segments[i].offset-lastEnd);
			if(i == 0 || !m_segments[i-1].client)
				freeSegsUnallocated << fs;
			else
			{
				fs.affectedClient = m_segments[i-1].client;
				freeSegs << fs;
			}
		}
		lastEnd = m_segments[i].offset + m_segments[i].bytes;
	}
	if (lastEnd < m_nTotal)
	{
		FreeSegment fs(lastEnd, m_nTotal - lastEnd);
		if (m_segments[m_segments.size()-1].client)
		{
			fs.affectedClient = m_segments[m_segments.size()-1].client;
			freeSegs << fs;
		}
		else
			freeSegsUnallocated << fs;
	}

	// 2) sort them
	qSort(freeSegs.begin(), freeSegs.end());
	qSort(freeSegsUnallocated.begin(), freeSegsUnallocated.end());

	Segment seg;
	qlonglong bytes;

	seg.color = allocateSegmentColor();
	seg.bytes = 0;
	seg.urlIndex = urlIndex;

	if (!freeSegsUnallocated.isEmpty())
	{
		// 3) use the smallest unallocated segment
		seg.offset = freeSegsUnallocated[0].offset;
		bytes = freeSegsUnallocated[0].bytes;
	}
	else if(!freeSegs.isEmpty())
	{
		// 4) split the biggest free spot into halves
		FreeSegment& fs = freeSegs[freeSegs.size()-1];
		//int odd = fs.bytes % 2;
		qlonglong half = fs.bytes / 2;

		if (half <= getSettingsValue("httpftp/minsegsize").toInt())
		{
			// remove the desired urlIndex from the list of active URLs
			m_listActiveSegments.removeOne(urlIndex);
			return;
		}

		// notify the active thread of the change
		qlonglong from = fs.affectedClient->rangeFrom();
		qlonglong to = fs.affectedClient->rangeTo();
		if (to == -1)
			to = m_nTotal;
		fs.affectedClient->setRange(from, to = to - half);

		seg.offset = to;
		bytes = half;
	}
	else
	{
		// This should never happen
		return;
	}

	// start a new download thread
	startSegment(seg, bytes);
	m_segments << seg;
}

void CurlDownload::stopSegment(int index)
{
	Segment& s = m_segments[index];
	if (!s.client)
		return;
	updateSegmentProgress();
	s.urlIndex = -1;
	m_master->removeTransfer(s.client);
	s.client->stop();
	s.client = 0;
	simplifySegments(m_segments);
}

QColor CurlDownload::allocateSegmentColor()
{
	for(size_t i=0;i<sizeof(g_colors)/sizeof(g_colors[0]);i++)
	{
		bool bFound = false;
		for(int j=0;j<m_segments.size();j++)
		{
			if(m_segments[j].client && m_segments[j].color == g_colors[i])
			{
				bFound = true;
				break;
			}
		}

		if(!bFound)
			return g_colors[i];
	}

	return QColor(rand()%256, rand()%256, rand()%256);
}

QObject* CurlDownload::createDetailsWidget(QWidget* w)
{
	HttpDetails* d = new HttpDetails(w);
	d->setDownload(this);
	return d;
}


