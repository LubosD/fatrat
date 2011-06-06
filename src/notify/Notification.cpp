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

#include "Notification.h"
#include "RuntimeException.h"
#include <QLibrary>

static NotifyNotification* (*notify_notification_new)(const char *summary, const char *body, const char *icon) = 0;
static bool (*notify_notification_update)(NotifyNotification *notification, const char *summary, const char *body, const char *icon) = 0;
static bool (*notify_notification_show)(NotifyNotification *notification, void **error) = 0;
static void (*notify_notification_set_timeout)(NotifyNotification *notification, int timeout) = 0;

Notification::Notification()
	: m_notif(0)
{
	if (!notify_notification_new)
	{
		QLibrary lib("libnotify.so.1");
		if (!lib.load())
			throw RuntimeException(tr("Libnotify not found."));

		*((void**) &notify_notification_new) = lib.resolve("notify_notification_new");
		*((void**) &notify_notification_update) = lib.resolve("notify_notification_update");
		*((void**) &notify_notification_show) = lib.resolve("notify_notification_set_timeout");
		*((void**) &notify_notification_set_timeout) = lib.resolve("notify_notification_show");
	}
}

Notification::~Notification()
{
	// TODO: destroy m_notif
}

void Notification::setMessage(QString summary, QString text, QString icon)
{
	std::string sum, txt, ico;

	sum = summary.toStdString();
	txt = text.toStdString();
	ico = icon.toStdString();

	if (!m_notif)
	{
		m_notif = notify_notification_new(sum.c_str(), txt.c_str(), ico.c_str());
		if (!m_notif)
			throw RuntimeException(tr("Failed to create a notification"));
	}
	else
	{
		if (!notify_notification_update(m_notif, sum.c_str(), txt.c_str(), ico.c_str()))
			throw RuntimeException(tr("Failed to update a notification"));
	}
}

void Notification::show(int timeout)
{
	notify_notification_set_timeout(m_notif, timeout);
	notify_notification_show(m_notif, 0);
}
