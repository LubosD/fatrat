/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#include "ExtendedAttributes.h"
#include <string>

const char* ExtendedAttributes::ATTR_COMMENT = "user.xdg.comment";
const char* ExtendedAttributes::ATTR_ORIGIN_URL = "user.xdg.origin.url";

#if !defined(HAVE_XATTR_H)
bool ExtendedAttributes::setAttribute(QString file, QString name, QByteArray value)
{
	return false;
}
#else

#include <attr/xattr.h>

bool ExtendedAttributes::setAttribute(QString file, QString name, QByteArray value)
{
	std::string path = file.toStdString();
	std::string sname = name.toStdString();

	return !setxattr(path.c_str(), sname.c_string(), value.constData(), value.size(), 0);
}

#endif

