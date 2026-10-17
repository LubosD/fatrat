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

#ifndef PYTHONEXTRACTOR_H
#define PYTHONEXTRACTOR_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QMap>
#include <QRegularExpression>
#include <string>

#include "PythonPersistentVariables.h"
#include "StaticTransferMessage.h"
#include "Transfer.h"
#include "python/PyRef.h"
#include "python/PluginLease.h"

class PyPlugin;

/* A link that has to be turned into other links first.
 *
 * An extractor plugin is handed a URL -- a folder listing, a "download all"
 * page, a container file -- and answers with the links inside it. This
 * transfer then replaces itself with one transfer per link, so what the user
 * ends up with is ordinary transfers.
 */
class PythonExtractor : public StaticTransferMessage<Transfer>, public PythonPersistentVariables
{
Q_OBJECT
public:
	PythonExtractor(const char* identifier);
	virtual ~PythonExtractor();

	static Transfer* createInstance(const EngineEntry* e) { return new PythonExtractor(e->shortName); }
	static int acceptable(QString uri, bool, const EngineEntry* e);
	static int acceptableFor(const QString& identifier, const QString& uri);

	virtual void speeds(int& down, int& up) const { down = up = 0; }
	virtual qulonglong total() const { return 0; }
	virtual qulonglong done() const { return 0; }

	virtual QString object() const { return QString(); }
	virtual void setObject(QString) {}
	virtual QString myClass() const { return m_strIdentifier; }
	virtual QString name() const;
	virtual void init(QString source, QString target);
	virtual void changeActive(bool nowActive);

	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;

	struct PyEngine
	{
		PyEngine() : ownAcceptable(false) {}

		std::string name, identifier;
		QRegularExpression regexp;
		bool ownAcceptable;
		QString targetClass; // engine for the extracted links, or empty to detect
		PyRef cls, probe;
	};

	static QMap<QString, PyEngine>& engines();
protected:
	void finishedExtraction(const QStringList& links);
	void failWith(const QString& message);
private:
	PluginLease m_lease; // declared first: released last
	QString m_strIdentifier, m_strUrl, m_strTarget;

	PyPlugin* m_plugin;
	PyRef m_pluginObject, m_task;
};

#endif // PYTHONEXTRACTOR_H
