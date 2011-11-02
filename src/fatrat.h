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

#ifndef _FATRAT_H
#define _FATRAT_H

#include <QString>
#include <QThread>
#include <QUuid>
#include <QList>
#include <QVariant>
#include <QFile>
#include <QIcon>
#include <QNetworkProxy>
#include "DelayedIcon.h"

#define VERSION "1.2.0-beta1"

QString formatSize(qulonglong size, bool persec = false);
QString formatTime(qulonglong secs);
QString getDataFileDir(QString dir, QString fileName = QString());
QWidget* getMainWindow();
void recursiveRemove(QString what);
bool openDataFile(QFile* file, QString filePath);
QStringList listDataDir(QString path);
bool programHasGUI();
void restartApplication();

void addStatusWidget(QWidget* widget, bool bRight);
void removeStatusWidget(QWidget* widget);

class Sleeper : public QThread
{
public:
	static void sleep(unsigned long secs) {QThread::sleep(secs);}
	static void msleep(unsigned long msecs) {QThread::msleep(msecs);}
	static void usleep(unsigned long usecs) {QThread::usleep(usecs);}
};

struct PluginInfo
{
	const char* version;
	QString name, author, website;
};

class Transfer;
class Queue;

struct MenuAction
{
	DelayedIcon icon;
	QString strName;
	void (*lpfnTriggered)(Transfer* t, Queue* q);
};

void addMenuAction(const MenuAction& action);

enum FtpMode { FtpActive = 0, FtpPassive };

#endif
