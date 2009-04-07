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

#ifndef SETTINGSJABBERFORM_H
#define SETTINGSJABBERFORM_H
#include <QObject>
#include "Proxy.h"
#include "config.h"
#include "WidgetHostChild.h"
#include "ui_SettingsJabberForm.h"

#ifndef WITH_JABBER
#	error This file is not supposed to be included!
#endif

class SettingsJabberForm : public QObject, public WidgetHostChild, Ui_SettingsJabberForm
{
Q_OBJECT
public:
	SettingsJabberForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
	static WidgetHostChild* create(QWidget* w, QObject* parent) { return new SettingsJabberForm(w, parent); }
private:
	QList<Proxy> m_listProxy;
};

#endif
