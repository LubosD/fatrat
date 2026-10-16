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


#include "TorrentDownload.h"
#include "TorrentDetails.h"
#include "TorrentOptsWidget.h"
#include <QIcon>
#include <QMenu>

void TorrentDownload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Force announce"));
	connect(a, SIGNAL(triggered()), this, SLOT(forceReannounce()));
	a = menu.addAction(QIcon(":/menu/reload.png"), tr("Recheck files"));
	a->setDisabled(isActive());
	connect(a, SIGNAL(triggered()), this, SLOT(forceRecheck()));
}

QObject* TorrentDownload::createDetailsWidget(QWidget* widget)
{
	return new TorrentDetails(widget, this);
}

WidgetHostChild* TorrentDownload::createOptionsWidget(QWidget* w)
{
	return new TorrentOptsWidget(w, this);
}

void TorrentWorker::setDetailsObject(TorrentDetails* d)
{
	connect(&m_timer, SIGNAL(timeout()), d, SLOT(refresh()));
}
