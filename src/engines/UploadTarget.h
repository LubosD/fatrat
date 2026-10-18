/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef UPLOADTARGET_H
#define UPLOADTARGET_H
#include <QUrl>
#include <QJsonObject>

// Composes the upload target URL for a partial options update.
// A "target" that carries user info uses it; otherwise the current credentials
// are kept. Explicit "username"/"password" keys always win.
inline QUrl composeUploadTarget(const QUrl& current, const QJsonObject& values)
{
	QUrl url = current;

	if (values.contains("target"))
	{
		QUrl newUrl(values.value("target").toString().trimmed());
		if (newUrl.userInfo().isEmpty())
		{
			newUrl.setUserName(current.userName());
			newUrl.setPassword(current.password());
		}
		url = newUrl;
	}
	if (values.contains("username"))
		url.setUserName(values.value("username").toString());
	if (values.contains("password"))
		url.setPassword(values.value("password").toString());
	return url;
}

#endif
