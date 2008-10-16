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


#include "CurlDownload.h"
#include "Settings.h"
#include "RuntimeException.h"
#include "GeneralDownloadForms.h"
#include "HttpFtpSettings.h"
#include "tools/HashDlg.h"
#include "CurlPoller.h"
#include "Auth.h"
#include <QMessageBox>
#include <QMenu>
#include <QtDebug>

static const QColor g_colors[] = { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow, Qt::darkRed,
	Qt::darkGreen, Qt::darkBlue, Qt::darkCyan, Qt::darkMagenta, Qt::darkYellow };

CurlDownload::CurlDownload()
	: m_nTotal(0), m_bAutoName(false), m_master(0), m_nameChanger(0)
{
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
	
	obj.url = uri;
	
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
	
	if(m_strFile.isEmpty())
		generateName();
	
	autoCreateSegment();
}

void CurlDownload::generateName()
{
	QString name;
	if(!m_urls.isEmpty())
		name = QFileInfo(m_urls[0].url.path()).fileName();
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
	CurlPoller::init();
	new CurlPoller;
	
	SettingsItem si;
	si.icon = QIcon(":/fatrat/httpftp.png");
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
		
		if(m_segments.isEmpty())
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

run_segments:
		for(int i=0;i<m_segments.size();i++)
		{
			Segment& csg = m_segments[i];
			
			if(csg.urlIndex < 0 || csg.urlIndex >= m_urls.size() || csg.client != 0)
				continue;
			
			UrlClient* client = new UrlClient;
			QFile* file = new QFile(client);
			qlonglong rangeFrom, rangeTo = -1;
			
			rangeFrom = csg.offset+csg.bytes;
			if(i+1 < m_segments.size())
				rangeTo = m_segments[i+1].offset;
			
			client->setRange(rangeFrom, rangeTo);
			client->setSourceObject(m_urls[csg.urlIndex]);
			client->setTargetObject(file);
			
			connect(client, SIGNAL(renameTo(QString)), this, SLOT(clientRenameTo(QString)));
			connect(client, SIGNAL(logMessage(QString)), this, SLOT(clientLogMessage(QString)));
			connect(client, SIGNAL(done(QString)), this, SLOT(clientDone(QString)));
			connect(client, SIGNAL(totalSizeKnown(qlonglong)), this, SLOT(clientTotalSizeKnown(qlonglong)));
			
			file->setFileName(filePath());
			file->open(QIODevice::WriteOnly);
			
			Segment sg;
			sg.offset = rangeFrom;
			sg.bytes = 0;
			sg.urlIndex = csg.urlIndex;
			sg.client = client;
			sg.color = allocateSegmentColor();
			
			client->setPollingMaster(m_master);
			client->start();
			m_master->addTransfer(client);
			
			csg.urlIndex = -1;
			m_segments.insert(i+1, sg);
			
			i++;
			active++;
		}
		
		if(!active)
		{
			autoCreateSegment();
			goto run_segments;
		}
		
		m_timer.start(500);
		
		for(int i=0;i<m_segments.size();i++)
			qDebug() << "Segment " << i << ": " << m_segments[i].offset << ", +" << m_segments[i].bytes;
	}
	else
	{
		m_segmentsLock.lockForWrite();
		for(int i=0;i<m_segments.size();i++)
		{
			if(!m_segments[i].client)
				continue;
			m_master->removeTransfer(m_segments[i].client);
			m_segments[i].client->stop();
			m_segments[i].client->deleteLater();
			m_segments[i].client = 0;
			m_segments[i].color = QColor();
		}
		simplifySegments(m_segments);
		m_segmentsLock.unlock();
		m_nameChanger = 0;
		m_timer.stop();
		
		CurlPoller::instance()->removeTransfer(m_master);
		delete m_master;
		m_master = 0;
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
		data.urlIndex = getXMLProperty(segment, "urlindex").toInt();
		data.client = 0;
		
		segment = segment.nextSiblingElement("segment");
		m_segments << data;
	}
	
	if(m_strFile.isEmpty())
		generateName();
	
	if(m_segments.isEmpty())
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
		
		setXMLProperty(doc, sub, "address", m_urls[i].url.toString());
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
		setXMLProperty(doc, sub, "urlindex", QString::number(s.urlIndex));
		
		subSegments.appendChild(sub);
	}
	m_segmentsLock.unlock();
	map.appendChild(subSegments);
}

void CurlDownload::autoCreateSegment()
{
	if(m_segments.isEmpty())
	{
		Segment s;
		QFileInfo fi(filePath());
		
		s.offset = s.bytes = 0;
		s.urlIndex = 0;
		s.client = 0;
		
		if(fi.exists())
			s.bytes = fi.size();
		
		m_segments << s;
	}
	else
	{
		// we have to find some free space and allocate a URL
		qlonglong pos = 0;
		bool solved = false;
			
		for(int i=0;i<m_segments.size();i++)
		{
			if(m_segments[i].offset > pos)
			{
				// a free spot
				solved = true;
				
				Segment sg;
				sg.offset = sg.bytes = 0;
				sg.client = 0;
				sg.urlIndex = 0;
				
				if(i > 0)
					sg.offset = m_segments[i-1].offset + m_segments[i-1].bytes;
				
				m_segments.insert(i, sg);
				break;
			}
		}
		
		if(!solved)
		{
			// create a segment at the end
			Segment sg;
			sg.bytes = 0;
			sg.client = 0;
			sg.urlIndex = 0;
			sg.offset = m_segments.last().offset + m_segments.last().bytes;
			
			m_segments << sg;
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

/*void CurlDownload::switchMirror()
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
}*/

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
	return new HttpOptsWidget(w,this);
}

void CurlDownload::simplifySegments(QList<CurlDownload::Segment>& retval)
{
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
	m_nTotal = bytes;
}

void CurlDownload::clientDone(QString error)
{
	UrlClient* client = static_cast<UrlClient*>(sender());
	m_segmentsLock.lockForWrite();
	
	for(int i=0;i<m_segments.size();i++)
	{
		if(m_segments[i].client == client)
		{
			m_segments[i].bytes = client->progress();
			m_segments[i].client = 0;
			m_segments[i].color = QColor();
		}
	}
	
	simplifySegments(m_segments);
	
	m_segmentsLock.unlock();
	
	m_master->removeTransfer(client);
	client->deleteLater();
	
	qulonglong d = done();
	if(d == total() && d)
	{
		setState(Completed);
	}
	else if(!error.isNull())
	{
		// TODO: show error
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
			setState(Failed);
	}
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

