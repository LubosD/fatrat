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

#include "HttpDetails.h"
#include "CurlDownload.h"
#include "Settings.h"
#include "fatrat.h"
#include "GeneralDownloadForms.h"
#include "HttpMirrorsDlg.h"
#include <QPainter>
#include <QLinearGradient>
#include <QFile>
#include <QMessageBox>

HttpDetails::HttpDetails(QWidget* w) : m_download(0)
{
	setupUi(w);

	connect(pushSegmentDelete, SIGNAL(clicked()), this, SLOT(deleteSegment()));
	connect(pushUrlAdd, SIGNAL(clicked()), this, SLOT(addUrl()));
	connect(pushUrlEdit, SIGNAL(clicked()), this, SLOT(editUrl()));
	connect(pushUrlDelete, SIGNAL(clicked()), this, SLOT(deleteUrl()));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	connect(pushMirrors, SIGNAL(clicked()), this, SLOT(mirrorSearch()));

	m_menu.addAction(tr("Add new URL..."), this, SLOT(addSegmentUrl()));
	m_separator = m_menu.addSeparator();
	pushSegmentAdd->setMenu(&m_menu);

	m_timer.start(getSettingsValue("gui_refresh").toInt());
	treeActiveSegments->setColumnWidth(0, 500);
}
void HttpDetails::setDownload(CurlDownload* d)
{
	m_download = d;
	widgetSegments->setDownload(d);
	refresh();
}

void HttpDetails::addSegment()
{
	QAction* act = static_cast<QAction*>(sender());
	int urlIndex = act->data().toInt();
	m_download->m_listActiveSegments << urlIndex;
	if (m_download->isActive() && m_download->total())
		m_download->startSegment(urlIndex);
	refresh();
}

void HttpDetails::deleteSegment()
{
	QTreeWidgetItem* item = treeActiveSegments->currentItem();
	if (!item)
		return;
	int urlIndex = item->data(0, Qt::UserRole).toInt();
	if (m_download->isActive())
	{
		int segIndex = item->data(1, Qt::UserRole).toInt();
		m_download->stopSegment(segIndex);
	}
	m_download->m_listActiveSegments.removeOne(urlIndex);
	delete item;
}

void HttpDetails::addUrl()
{
	HttpUrlOptsDlg dlg(treeActiveSegments->parentWidget());
	if (dlg.exec() != QDialog::Accepted)
		return;

	UrlClient::UrlObject obj;

	obj.url = dlg.m_strURL;
	obj.url.setUserName(dlg.m_strUser);
	obj.url.setPassword(dlg.m_strPassword);
	obj.strReferrer = dlg.m_strReferrer;
	obj.ftpMode = dlg.m_ftpMode;
	obj.proxy = dlg.m_proxy;
	obj.strBindAddress = dlg.m_strBindAddress;

	listUrls->addItem(dlg.m_strURL);
	m_download->m_urls << obj;
}

void HttpDetails::editUrl()
{
	HttpUrlOptsDlg dlg(treeActiveSegments->parentWidget());
	int row = listUrls->currentRow();
	if (row < 0)
		return;
	UrlClient::UrlObject& obj = m_download->m_urls[row];

	QUrl temp = obj.url;
	temp.setUserInfo(QString());
	dlg.m_strURL = temp.toString();
	dlg.m_strUser = obj.url.userName();
	dlg.m_strPassword = obj.url.password();
	dlg.m_strReferrer = obj.strReferrer;
	dlg.m_ftpMode = obj.ftpMode;
	dlg.m_proxy = obj.proxy;
	dlg.m_strBindAddress = obj.strBindAddress;

	if(dlg.exec() == QDialog::Accepted)
	{
		obj.url = dlg.m_strURL;
		obj.url.setUserName(dlg.m_strUser);
		obj.url.setPassword(dlg.m_strPassword);
		obj.strReferrer = dlg.m_strReferrer;
		obj.ftpMode = dlg.m_ftpMode;
		obj.proxy = dlg.m_proxy;
		obj.strBindAddress = dlg.m_strBindAddress;

		listUrls->item(row)->setText(dlg.m_strURL);

		if (m_download->isActive())
		{
			int stopped = 0;
			for (int i = 0; i < m_download->m_segments.size(); i++)
			{
				CurlDownload::Segment& s = m_download->m_segments[i];
				if (s.client && s.urlIndex == row)
				{
					// restart active segments
					m_download->stopSegment(i, true);
					stopped++;
					i = 0;
				}
			}
			while (stopped--)
				m_download->startSegment(row);
		}
	}
}

void HttpDetails::deleteUrl()
{
	int row = listUrls->currentRow();
	if (m_download->m_urls.size() <= 1)
		return;

	if(row >= 0)
	{
		delete listUrls->takeItem(row);
		m_download->m_urls.removeAt(row);

		if (m_download->isActive())
		{
			// stop active segments
			for (int i = 0; i < m_download->m_segments.size(); i++)
			{
				CurlDownload::Segment& s = m_download->m_segments[i];
				if (s.urlIndex == row && s.client)
				{
					m_download->stopSegment(i);
					i = 0;
				}
			}
			// renumber segments
			for (int i = 0; i < m_download->m_segments.size(); i++)
			{
				CurlDownload::Segment& s = m_download->m_segments[i];
				if (s.urlIndex > row)
					s.urlIndex--;
			}
		}
		for (int i = 0; i < m_download->m_listActiveSegments.size(); i++)
		{
			int& n = m_download->m_listActiveSegments[i];
			if (n > row)
				n--;
			else if (n == row)
				m_download->m_listActiveSegments.removeAt(i--);
		}
	}
}

void HttpDetails::addSegmentUrl()
{
	HttpUrlOptsDlg dlg(treeActiveSegments->parentWidget());
	if (dlg.exec() != QDialog::Accepted)
		return;
}


void HttpDetails::refresh()
{
	QReadLocker l(&m_download->m_segmentsLock);
	if (m_download->isActive())
	{
		for (int i=0,j=0;i<m_download->m_segments.size();i++)
		{
			const CurlDownload::Segment& s = m_download->m_segments[i];
			if (!s.client || s.urlIndex < 0)
				continue;
			QTreeWidgetItem* item;
			QString url = m_download->m_urls[s.urlIndex].url.toString();
			GradientWidget* gradient;

			if (j < treeActiveSegments->topLevelItemCount())
				item = treeActiveSegments->topLevelItem(j);
			else
			{
				item = new QTreeWidgetItem(treeActiveSegments);
				treeActiveSegments->addTopLevelItem(item);
			}
			item->setData(0, Qt::UserRole, s.urlIndex);
			item->setData(1, Qt::UserRole, i);
			item->setText(0, url);
			gradient = static_cast<GradientWidget*>(treeActiveSegments->itemWidget(item, 1));

			if (!gradient)
			{
				gradient = new GradientWidget(s.color);
				treeActiveSegments->setItemWidget(item, 1, gradient);
			}
			else if (gradient->color() != s.color)
			{
				gradient->setColor(s.color);
			}

			QString size;
			qlonglong from, to;
			int down, up;

			from = s.client->rangeFrom();
			to = s.client->rangeTo();

			if (to == -1)
				size = "∞";
			else
				size = formatSize(to-from);
			item->setText(2, size);

			s.client->speeds(down, up);
			if (s.client->progress())
				item->setText(3, QString("%1%").arg(int(100 / (double(to-from) / s.client->progress()) )));
			else
				item->setText(3, QString());
			item->setText(4, formatSize(down)+"/s");
			j++;
		}

		int count;
		while ( (count = treeActiveSegments->topLevelItemCount()) > m_download->m_listActiveSegments.size())
			delete treeActiveSegments->takeTopLevelItem(count-1);
	}
	else
	{
		for (int i=0;i<m_download->m_listActiveSegments.size();i++)
		{
			int urlIndex = m_download->m_listActiveSegments[i];
			QTreeWidgetItem* item;
			QString url = m_download->m_urls[urlIndex].url.toString(); // TODO - this is a point of crash due to subclassing issues

			if (i < treeActiveSegments->topLevelItemCount())
				item = treeActiveSegments->topLevelItem(i);
			else
			{
				item = new QTreeWidgetItem(treeActiveSegments);
				treeActiveSegments->addTopLevelItem(item);
			}
			item->setData(0, Qt::UserRole, urlIndex);
			item->setText(0, url);
			treeActiveSegments->removeItemWidget(item, 1);
			item->setText(2, QString());
			item->setText(3, QString());
			item->setText(4, QString());
		}

		int count;
		while ( (count = treeActiveSegments->topLevelItemCount()) > m_download->m_listActiveSegments.size())
			delete treeActiveSegments->takeTopLevelItem(count-1);
	}

	for (int i=0;i<m_download->m_urls.size();i++)
	{
		QString url = m_download->m_urls[i].url.toString();
		QListWidgetItem* item;

		if (i < listUrls->count())
			item = listUrls->item(i);
		else
		{
			item = new QListWidgetItem(listUrls);
			listUrls->addItem(item);
		}
		item->setText(url);
	}
	int count;
	while ( (count = listUrls->count()) > m_download->m_urls.size())
		delete listUrls->takeItem(count-1);

	for (int i=0;i<m_download->m_urls.size();i++)
	{
		QString url = m_download->m_urls[i].url.toString();
		QAction* a;
		if(url.size() > 50)
		{
			url.resize(47);
			url += "...";
		}

		if (m_menuActions.size() <= i)
		{
			a = m_menu.addAction(url, this, SLOT(addSegment()));
			m_menuActions << a;
		}
		else
		{
			a = m_menuActions[i];
			a->setText(url);
		}
		a->setData(i);
	}
	for(int i=m_download->m_urls.size();i<m_menuActions.size();i++)
	{
		m_menu.removeAction(m_menuActions[i]);
		m_menuActions.removeAt(i--);
	}
}

HttpDetails::GradientWidget::GradientWidget(QColor color) : m_color(color)
{
}

void HttpDetails::GradientWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	QLinearGradient gradient(0, 0, 0, height());

	gradient.setColorAt(0, m_color.lighter(200));
	gradient.setColorAt(0.5, m_color);
	gradient.setColorAt(1, m_color);

	painter.fillRect(rect(), QBrush(gradient));
}

void HttpDetails::mirrorSearch()
{
	// get source and effective URLs
	QSet<QString> urls;

	for (int i=0;i<m_download->m_urls.size();i++)
	{
		UrlClient::UrlObject& obj = m_download->m_urls[i];
		if (!obj.effective.isEmpty())
			urls << obj.effective.toString();
		else
			urls << obj.url.toString();
	}
	// find compatible mirror groups
	QSet<QString> groups;
	QMap<QString,QStringList> mirrors = loadMirrors();
	QMap<QString,QString> append;

	foreach (QString url, urls)
	{
		for(QMap<QString,QStringList>::iterator it=mirrors.begin(); it != mirrors.end(); it++)
		{
			foreach (QString murl, it.value())
			{
				if (url.startsWith(murl))
				{
					groups << it.key();
					append[it.key()] = url.mid(murl.size());
					it.value().removeAll(murl);
					break;
				}
			}
		}
	}

	if (groups.isEmpty())
		QMessageBox::warning(listUrls->parentWidget(), "FatRat", tr("No mirrors found."));
	else
	{
		HttpMirrorsDlg dlg(listUrls->parentWidget());
		dlg.load(mirrors, groups);
		if (dlg.exec() == QDialog::Accepted)
		{
			QMap<QString,QStringList> urls = dlg.pickedUrls();
			for(QMap<QString,QStringList>::iterator it=urls.begin();it!=urls.end();it++)
			{
				QString app = append[it.key()];
				foreach (QString url, it.value())
				{
					UrlClient::UrlObject obj;
					obj.url = url + app;
					m_download->m_urls << obj;
				}
			}
			refresh();
		}
	}
}

QMap<QString,QStringList> HttpDetails::loadMirrors()
{
	QFile file;
	QMap<QString,QStringList> rv;

	if(!openDataFile(&file, "/data/mirrors.txt"))
		return rv;
	QString nextGrp;
	QStringList list;
	QRegExp re("\\[([^\\]]+)\\]");

	while (!file.atEnd())
	{
		QString line = file.readLine().trimmed();
		if (line.isEmpty())
			continue;
		if (re.exactMatch(line))
		{
			if (!nextGrp.isEmpty())
				rv[nextGrp] = list;
			list.clear();
			nextGrp = re.cap(1);
		}
		else
			list << line;
	}
	if (!list.isEmpty())
		rv[nextGrp] = list;

	return rv;
}
