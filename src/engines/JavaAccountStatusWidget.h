/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef JAVAACCOUNTSTATUSWIDGET_H
#define JAVAACCOUNTSTATUSWIDGET_H
#include "config.h"

#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QFrame>
#include "ui_JavaAccountStatusWidget.h"
#include "java/JAccountStatusPlugin.h"
#include <QListWidgetItem>
#include <QMap>

class JavaAccountStatusWidget : public QFrame, Ui_JavaAccountStatusWidget
{
Q_OBJECT
public:
	JavaAccountStatusWidget(QWidget* parent);
	~JavaAccountStatusWidget();
private slots:
	void accountBalanceReceived(JAccountStatusPlugin::AccountState state, QString bal);
private:
	QList<JAccountStatusPlugin*> m_plugins;
	QMap<QString,QTreeWidgetItem*> m_items;
};

#endif // JAVAACCOUNTSTATUSWIDGET_H
