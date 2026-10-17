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

#ifndef PYTHONDOWNLOAD_H
#define PYTHONDOWNLOAD_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QMap>
#include <QMutex>
#include <QRegularExpression>
#include <QUrl>
#include <string>

#include "PythonPersistentVariables.h"
#include "WidgetHostChild.h"
#include "engines/CurlDownload.h"
#include "python/PyRef.h"
#include "python/PluginLease.h"

class PyPlugin;

/* A transfer handled by a Python download plugin.
 *
 * The plugin only resolves the link: it scrapes, waits, solves a captcha,
 * and hands back a real URL with the cookies it collected. The download
 * itself is CurlDownload's, so a plugin's transfer gets FatRat's segmenting,
 * speed limits and resume for free -- which is why this derives from it
 * rather than downloading anything itself.
 */
class PythonDownload : public CurlDownload, public PythonPersistentVariables
{
Q_OBJECT
public:
	PythonDownload(const char* identifier);
	virtual ~PythonDownload();

	virtual void init(QString source, QString target);
	virtual QString myClass() const;
	virtual QString name() const;

	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual void changeActive(bool bActive);

#ifdef WITH_GUI
	virtual QObject* createDetailsWidget(QWidget*) { return 0; }
	virtual WidgetHostChild* createOptionsWidget(QWidget* w);
	virtual void fillContextMenu(QMenu&) {}
#endif

	virtual QString object() const { return m_strTarget; }
	virtual void setObject(QString newdir);

	virtual qulonglong done() const;
	virtual void setState(State newState);
	virtual QString remoteURI() const;

	static Transfer* createInstance(const EngineEntry* e) { return new PythonDownload(e->shortName); }
	static int acceptable(QString uri, bool, const EngineEntry* e);
	// Same question, asked about a plugin by identifier (used by the UI).
	static int acceptableFor(const QString& identifier, const QString& uri);

	/* One registered plugin class. Filled in by PluginRegistry at startup;
	 * `name` and `identifier` are std::string because EngineEntry keeps
	 * borrowed char pointers into them for the life of the process.
	 */
	struct PyEngine
	{
		PyEngine() : singleTransfer(false), truncate(false), ownAcceptable(false) {}

		std::string name, identifier;
		QRegularExpression regexp;
		bool singleTransfer, truncate, ownAcceptable;
		PyRef cls;   // the plugin class, instantiated per transfer
		PyRef probe; // one instance, for acceptable()
	};

	static QMap<QString, PyEngine>& engines();

	// Called by the plugin through start_download().
	void startDownload(const QString& url, const QString& referrer, const QString& userAgent,
			   const QString& postData, const QString& fileName);
	void setName(const QString& name) { m_strName = name; }

	PyPlugin* plugin() const { return m_plugin; }
protected:
	void deriveName();
	void failWith(const QString& message);
private:
	PluginLease m_lease; // declared first: released last
	QString m_strIdentifier;
	QString m_strOriginal, m_strName, m_strTarget;
	QUrl m_downloadUrl;
	bool m_bHasLock, m_bTruncate, m_bFinalChecked;

	PyPlugin* m_plugin;
	PyRef m_pluginObject; // keeps the Python half alive
	PyRef m_task;         // the running coroutine, if any

	static QMap<QString, QMutex*> m_mutexes;

	friend class PyPlugin;
};

#endif // PYTHONDOWNLOAD_H
