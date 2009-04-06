/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "HttpDetailsBar.h"
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include "CurlDownload.h"
#include "Settings.h"

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

void HttpDetailsBar::mousePressEvent(QMouseEvent* event)
{
	int x = event->x();
	bool found = false;
	
	for(int i=0;i<m_segs.size();i++)
	{
		if(x >= m_segs[i].first && x <= m_segs[i].second)
		{
			found = true;
			if(m_sel != i)
			{
				m_sel = i;
				update();
			}
			break;
		}
	}
	
	if(!found && m_sel != -1)
	{
		m_sel = -1;
		update();
	}
	
	if(event->button() == Qt::RightButton && m_download != 0 && m_download->isActive())
	{
		QReadLocker l(&m_download->m_segmentsLock);
		QMenu menu(this);
		
		if(m_sel >= 0 && m_sel < m_segs.size())
		{
			if(!m_download->m_segments[m_sel].client)
			{
				//if(m_download->m_segments[m_sel].urlIndex < 0)
					return;
				//menu.addAction(tr("Resume this segment"), this, SLOT(resumeSegment()));
			}
			else
			{
				//menu.addAction(tr("Pause this segment"), this, SLOT(pauseSegment()));
				menu.addAction(tr("Stop this segment"), this, SLOT(stopSegment()));
			}
		}
		else
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
		
		menu.exec(QCursor::pos());
	}
}

void HttpDetailsBar::createSegment()
{
	QAction* act = static_cast<QAction*>(sender());
	int url = act->data().toInt();
}

void HttpDetailsBar::stopSegment()
{
}

void HttpDetailsBar::pauseSegment()
{
}

void HttpDetailsBar::resumeSegment()
{
}

