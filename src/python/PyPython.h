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

#ifndef PYPYTHON_H
#define PYPYTHON_H

/* Shared helpers for the C++ side of the Python plugin API.
 *
 * Include this FIRST in any file under src/python/ that talks to Python: it
 * pulls in the Python headers, which must precede every Qt header (CPython
 * has struct members called `slots`). Files that moc has to read must not
 * include it at all -- they use PyRef instead.
 */

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <functional>

#include <QString>
#include <QVariant>

#include "PyRef.h"

// Text of a Python exception, including the traceback when there is one.
QString pyErrorText(const pybind11::error_already_set& e);

// Conversions. pyToVariant handles str/bytes/bool/int/float/None and
// lists/dicts thereof; anything else comes back as an invalid QVariant.
pybind11::object pyFromVariant(const QVariant& v);
QVariant pyToVariant(pybind11::handle h);

PyRef pyToRef(pybind11::handle h);
pybind11::object pyFromRef(const PyRef& ref);

// The shipped Python package, by submodule.
pybind11::module_ pyModuleFatrat();
pybind11::module_ pyModuleAsync();
pybind11::module_ pyModuleHttp();
pybind11::module_ pyModuleErrors();

// fatrat._async.Future, optionally with a callback invoked if the plugin (or
// FatRat) cancels it before it completes.
pybind11::object pyMakeFuture(std::function<void()> onCancel = std::function<void()>());
void pyResolveFuture(const pybind11::object& future, pybind11::object value);
// `exception` is a class name in fatrat.errors, e.g. "NetworkError".
void pyFailFuture(const pybind11::object& future, const char* exception, const QString& message);

// Run a plugin coroutine. `onDone(result, exception)` is called once, with
// one of the two set; `exception` is None on success.
pybind11::object pySpawnTask(pybind11::object coroutine,
	std::function<void(pybind11::object, pybind11::object)> onDone);

// Registered into the embedded _fatrat module (see PyModule.cpp).
void registerCoreBindings(pybind11::module_& m);
void registerPluginBindings(pybind11::module_& m);

#endif // PYPYTHON_H
