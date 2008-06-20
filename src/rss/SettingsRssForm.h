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

#ifndef SETTINGSRSSFORM_H
#define SETTINGSRSSFORM_H
#include "ui_SettingsRssForm.h"
#include "WidgetHostChild.h"
#include <QObject>

#include "RssFetcher.h"

class SettingsRssForm : public QObject, public WidgetHostChild, Ui_SettingsRssForm
{
Q_OBJECT
public:
	SettingsRssForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
	static WidgetHostChild* create(QWidget* w, QObject* parent) { return new SettingsRssForm(w, parent); }
protected slots:
	void feedAdd();
	void feedEdit();
	void feedDelete();
	
	void regexpAdd();
	void regexpEdit();
	void regexpDelete();
private:
	QList<RssFeed> m_feeds;
	QList<RssRegexp> m_regexps;
};

#endif
