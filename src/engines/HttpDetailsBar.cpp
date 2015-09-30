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

#include "HttpDetailsBar.h"
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QColor>
#include <QToolTip>
#include <QCursor>
#include "CurlDownload.h"
#include "Settings.h"
#include "fatrat.h"

HttpDetailsBar::HttpDetailsBar(QWidget* parent)
	: QWidget(parent), m_download(0), m_sel(-1)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
	m_timer.start(getSettingsValue("gui_refresh").toInt());
}

void HttpDetailsBar::setDownload(CurlDownload* d)
{
	m_download = d;
	update();
}

void HttpDetailsBar::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	const int width = this->width()-2;
	const int height = this->height();
	
	painter.setClipRegion(event->region());
	painter.fillRect(QRect(0,0,width,height), Qt::white);
	painter.drawRect(QRect(0, 0, width+1, height-1));
	
	m_segs.clear();
	
	if(!m_download)
		return;
	
	const qulonglong total = m_download->total();
	if(!total)
	{
		painter.drawText(rect(), Qt::AlignCenter, "?");
		return;
	}
	
	QReadLocker l(&m_download->m_segmentsLock);
	QPen dotted(Qt::white, 1, Qt::DotLine);
	
	for(int i=0;i<m_download->m_segments.size();i++)
	{
		const CurlDownload::Segment& sg = m_download->m_segments[i];
		painter.setPen(Qt::white);
		
		QRect rect = QRect(float(sg.offset)*width/total+1, 1, float(sg.bytes)*width/total, height-2);
		QLinearGradient gradient(0, 0, 0, height);
		
		if(sg.client != 0)
		{
			gradient.setColorAt(0, sg.color.lighter(200));
			gradient.setColorAt(0.5, sg.color);
			gradient.setColorAt(1, sg.color);
		}
		else
		{
			gradient.setColorAt(0, Qt::gray);
			gradient.setColorAt(0.5, Qt::black);
			gradient.setColorAt(1, Qt::black);
		}
		
		painter.fillRect(rect, QBrush(gradient));
		
		m_segs << QPair<int,int>(rect.x(), rect.right());
		
		if(m_sel == i)
		{
			painter.setPen(dotted);
			rect = rect.translated(3, 2);
			rect.setSize(QSize(rect.width()-7, rect.height()-5));
			painter.drawRect(rect);
		}
	}
}

void HttpDetailsBar::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_download)
		return;

	int seg = getSegment(event->x());

	if (seg == -1)
	{
		QToolTip::hideText();
		return;
	}

	QReadLocker l(&m_download->m_segmentsLock);
	if (seg >= m_download->m_segments.size())
	{
		QToolTip::hideText();
		return;
	}

	const CurlDownload::Segment& ss = m_download->m_segments[seg];
	QString text;
	if (ss.client)
	{
		QString url = m_download->m_urls[ss.urlIndex].url.toString();
		if (url.size() > 47)
		{
			url.resize(47);
			url += "...";
		}
		qlonglong progress;
		QString size = "?";
		int down, up;
		ss.client->speeds(down, up);
		progress = ss.client->progress();

		if (ss.client->rangeTo() != -1)
			size = formatSize(ss.client->rangeTo() - ss.client->rangeFrom());

		text = tr("Segment #%1\nDownload in progress\nURL: %2\nSize: %3\nSpeed: %4\nDone: %5")
		       .arg(seg).arg(url).arg(size).arg(formatSize(down)+"/s").arg(formatSize(progress));
	}
	else
	{
		text = tr("Segment #%1\nDownloaded data").arg(seg) + "\n" + formatSize(ss.bytes);
	}
	QToolTip::showText(mapToGlobal(event->pos()), text, this);
}

int HttpDetailsBar::getSegment(int x)
{
	for(int i=0;i<m_segs.size();i++)
	{
		if(x >= m_segs[i].first && x <= m_segs[i].second)
			return i;
	}
	return -1;
}

void HttpDetailsBar::mousePressEvent(QMouseEvent* event)
{
	int n = getSegment(event->x());

	if (n != m_sel)
	{
		m_sel = n;
		update();
	}

	if(event->button() == Qt::RightButton && m_download != 0 /*&& m_download->isActive()*/)
	{
		QMenu menu(this);
		
		m_download->m_segmentsLock.lockForRead();
		if(m_sel >= 0 && m_sel < m_segs.size() && m_download->m_segments[m_sel].client)
		{
			menu.addAction(tr("Delete this segment"), this, SLOT(stopSegment()));
		}
		//else
		{
			QMenu* seg = menu.addMenu(tr("New segment"));
			
			m_createX = event->x();
			
			for(int i=0;i<m_download->m_urls.size();i++)
			{
				QUrl url = m_download->m_urls[i].url;
				QString text;
				
				url.setUserInfo(QString());
				text = url.toString();
				
				if(text.size() > 50)
				{
					text.resize(47);
					text += "...";
				}
				seg->addAction(text, this, SLOT(createSegment()))->setData(i);
			}
		}
		m_download->m_segmentsLock.unlock();
		
		menu.exec(QCursor::pos());
	}
}

void HttpDetailsBar::createSegment()
{
	QAction* act = static_cast<QAction*>(sender());
	int url = act->data().toInt();
	m_download->m_listActiveSegments << url;
	if (m_download->isActive() && m_download->total())
		m_download->startSegment(url);
}

void HttpDetailsBar::stopSegment()
{
	m_download->m_listActiveSegments.removeOne(m_download->m_segments[m_sel].urlIndex);
	m_download->stopSegment(m_sel);
	update();
}
