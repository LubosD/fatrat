/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "MetalinkSettings.h"
#include "Settings.h"
#include "config.h"

MetalinkSettings::MetalinkSettings(QWidget* w, QObject* p)
	: QObject(p)
{
	setupUi(w);

#ifndef WITH_CURL
	radioUseHTTP->setDisabled(true);
#endif
#ifndef WITH_BITTORRENT
	radioUseTorrent->setDisabled(true);
#endif
}


void MetalinkSettings::load()
{
	int mode = getSettingsValue("metalink/mode").toInt();
	if (mode == 0)
		radioUseHTTP->setChecked(true);
	else if (mode == 1)
		radioUseTorrent->setChecked(true);
}

void MetalinkSettings::accepted()
{
	int mode = 0;
	if (radioUseTorrent->isChecked())
		mode = 1;
	setSettingsValue("metalink/mode", mode);
}

