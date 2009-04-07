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

#ifndef VIDEOFETCHER_H
#define VIDEOFETCHER_H
#include <QWidget>
#include "ui_VideoFetcher.h"

class QBuffer;

class VideoFetcher : public QWidget, Ui_VideoFetcher
{
Q_OBJECT
public:
	VideoFetcher();
	static QWidget* create() { return new VideoFetcher; }
	
	QString decodeYouTube(QByteArray data, QString url);
	QString decodeStreamCZ(QByteArray data, QString url);
public slots:
	void openLink(const QString& link);
	void decode();
	void download();
	void downloadDone(bool error);
private:
	void doNext();
	void finalize();
	
	QStringList m_videos;
	QStringList m_unsupp, m_err;
	QString m_now;
	int m_nextType;
	QBuffer* m_buffer;

public:
	struct Fetcher
	{
		const char* name;
		const char* re;
		QString (VideoFetcher::*lpfn)(QByteArray data, QString url);
	};
};

#endif
