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

#ifndef SETTINGSWEBFORM_H
#define SETTINGSWEBFORM_H
#include <QObject>
#include "fatrat.h"
#include "config.h"
#include "WidgetHostChild.h"
#include "ui_SettingsWebForm.h"

#ifndef WITH_REMOTE
#	error This file is not supposed to be included!
#endif

class SettingsWebForm : public QObject, public WidgetHostChild, Ui_SettingsWebForm
{
public:
	SettingsWebForm(QWidget* w, QObject* parent);
	virtual void load();
	virtual void accepted();
	static WidgetHostChild* create(QWidget* w, QObject* parent) { return new SettingsWebForm(w, parent); }
};

#endif
