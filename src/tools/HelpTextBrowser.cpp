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

#include "HelpTextBrowser.h"
#include "HelpBrowser.h"
#include <QDesktopServices>
#include <QtDebug>

HelpTextBrowser::HelpTextBrowser(QWidget* parent)
	: QTextBrowser(parent), m_engine(0)
{
}

void HelpTextBrowser::setEngine(QHelpEngine* engine)
{
	m_engine = engine;
}

QVariant HelpTextBrowser::loadResource(int type, const QUrl& name)
{
	QUrl url(name);
	
	if(name.scheme() == "http")
	{
		QMetaObject::invokeMethod(this, "setSource", Qt::QueuedConnection, Q_ARG(QUrl, m_urlLast));
		QDesktopServices::openUrl(name);
		return QVariant();
	}
	
	if(!m_engine)
		return QVariant();
	
	if(type == QTextDocument::HtmlResource)
	{
		QModelIndex index;
		QHelpContentWidget* w = m_engine->contentWidget();
		QItemSelectionModel* selmodel;
		
		m_urlLast = name;
		index = w->indexOf(m_urlLast);
		
		w->blockSignals(true);
		selmodel = w->selectionModel();
		selmodel->clearSelection();
		selmodel->select(index, QItemSelectionModel::Select);
		w->blockSignals(false);
	}
	
	if(url.isRelative())
		url = source().resolved(url);
	return m_engine->fileData(url);
}
