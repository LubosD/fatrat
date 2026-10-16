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

#ifndef PYVM_H
#define PYVM_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QString>
#include <QStringList>

/* Owns the embedded CPython interpreter.
 *
 * One instance per process, created on the main thread at startup and
 * destroyed at shutdown. The interpreter is initialized by the constructor;
 * if that fails, the failure is logged and available() stays false -- FatRat
 * then runs without Python extensions, exactly as it does with --no-python.
 *
 * The GIL is *not* held while FatRat is back in the Qt event loop: the
 * constructor releases it once the interpreter is up, and every call into
 * Python has to take it again (see PyGil in PyCall.h, from Phase 1 on).
 *
 * This header deliberately exposes no pybind11 or Python types, so that
 * fatrat.cpp and friends do not need the Python headers to boot the VM.
 */
class PyVM
{
public:
	PyVM();
	~PyVM();

	static PyVM* instance() { return m_instance; }
	// True once the interpreter is up and the shipped `fatrat` package imports.
	static bool available();

	// e.g. "3.12.11" -- empty when the interpreter is not available.
	QString pythonVersion() const { return m_strVersion; }
	// The search path handed to the interpreter, in order.
	QStringList searchPath() const { return m_searchPath; }

	// <prefix>/share/fatrat/data/python, or $FATRAT_PYTHON_PATH when set
	// (which is how an uninstalled build tree finds the shipped package).
	static QString runtimePath();
	// ~/.local/share/fatrat/plugins
	static QString pluginsPath();
	// ~/.local/share/fatrat/plugins/installed
	static QString installedPluginsPath();
private:
	bool buildSearchPath();
	void shutdown();

	bool m_bReady;
	void* m_mainThreadState; // PyThreadState*, saved while the GIL is released
	QString m_strVersion;
	QStringList m_searchPath;

	static PyVM* m_instance;
};

#endif // PYVM_H
