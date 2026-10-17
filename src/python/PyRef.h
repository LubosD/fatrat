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

#ifndef PYREF_H
#define PYREF_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

/* An owning reference to a Python object, with no Python headers attached.
 *
 * Qt's `signals`/`slots` keywords collide with member names in CPython's
 * headers, so any header that moc has to read -- anything with Q_OBJECT --
 * cannot include pybind11. PyRef is how such classes hold Python objects:
 * the reference counting lives in PyRef.cpp, where the Python headers are
 * included before any Qt one.
 *
 * Every operation takes the GIL itself, so a PyRef can be copied or destroyed
 * from code that knows nothing about Python (a Qt destructor, say).
 */
class PyRef
{
public:
	PyRef() : m_obj(0) {}
	PyRef(const PyRef& other);
	// Takes a PyObject*. `steal` consumes a new reference, otherwise one is added.
	explicit PyRef(void* obj, bool steal = false);
	~PyRef();

	PyRef& operator=(const PyRef& other);

	bool isNull() const { return m_obj == 0; }
	void* get() const { return m_obj; }
	void reset();
private:
	void* m_obj;
};

#endif // PYREF_H
