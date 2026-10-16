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

#ifndef PYPLUGIN_H
#define PYPLUGIN_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QMap>
#include <QObject>
#include <QString>

#include "PyRef.h"

class QNetworkAccessManager;
class QNetworkReply;

/* The native base class of every Python plugin (`_fatrat.PluginBase`).
 *
 * A plugin class written in Python derives from fatrat.Plugin, which derives
 * from this; pybind11 makes the Python object and this C++ object one and the
 * same, so there is no peer table to keep in sync (which is most of what the
 * old JNI layer did).
 *
 * Network access belongs here rather than in the plugin so that fetches use
 * FatRat's stack: this instance's cookie jar persists across the fetches of
 * one transfer, and the proxy and user agent are FatRat's.
 *
 * This header is read by moc, so it must not include pybind11 -- Python
 * objects are held as PyRef. Everything here runs on the main thread.
 */
class PyPlugin : public QObject
{
Q_OBJECT
public:
	PyPlugin();
	virtual ~PyPlugin();

	// Returns a fatrat._async.Future resolving to a fatrat.Response.
	// `data` non-null makes it a POST; `headers` is an optional dict.
	PyRef fetch(const QString& url, const PyRef& data, const PyRef& headers, const QString& method);

	// Drop everything in flight. Called when a transfer is stopped.
	virtual void abort();

	// True while a fetch is outstanding, i.e. the plugin is still working.
	virtual bool hasPendingWork() const { return !m_fetches.isEmpty(); }

	// Where a plugin's log() lands. The base writes to the global log;
	// transfer plugins override to write into the transfer's own log.
	virtual void logMessage(const QString& message);

	QString pluginName() const { return m_strPluginName; }
	void setPluginName(const QString& name) { m_strPluginName = name; }
private slots:
	void fetchFinished(QNetworkReply* reply);
private:
	QNetworkAccessManager* m_network;
	QMap<QNetworkReply*, PyRef> m_fetches; // reply -> Future
	QString m_strPluginName;
};

#endif // PYPLUGIN_H
