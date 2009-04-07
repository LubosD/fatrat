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

#include "Auth.h"
#include <QSettings>

extern QSettings* g_settings;

QList<Auth> Auth::loadAuths()
{
	QList<Auth> r;
	
	int count = g_settings->beginReadArray("httpftp/auths");
	for(int i=0;i<count;i++)
	{
		Auth auth;
		g_settings->setArrayIndex(i);
		
		auth.strRegExp = g_settings->value("regexp").toString();
		auth.strUser = g_settings->value("user").toString();
		auth.strPassword = g_settings->value("password").toString();
		
		r << auth;
	}
	g_settings->endArray();
	
	return r;
}

void Auth::saveAuths(const QList<Auth>& auths)
{
	g_settings->beginWriteArray("httpftp/auths");
	for(int i=0;i<auths.size();i++)
	{
		g_settings->setArrayIndex(i);
		g_settings->setValue("regexp", auths[i].strRegExp);
		g_settings->setValue("user", auths[i].strUser);
		g_settings->setValue("password", auths[i].strPassword);
	}
	g_settings->endArray();
}
