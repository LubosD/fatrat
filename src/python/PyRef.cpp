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

// Python headers before any Qt header; see PyRef.h.
#include <pybind11/pybind11.h>

#include "PyRef.h"

namespace py = pybind11;

PyRef::PyRef(void* obj, bool steal) : m_obj(obj)
{
	if (m_obj && !steal)
	{
		py::gil_scoped_acquire gil;
		Py_INCREF(static_cast<PyObject*>(m_obj));
	}
}

PyRef::PyRef(const PyRef& other) : m_obj(other.m_obj)
{
	if (m_obj)
	{
		py::gil_scoped_acquire gil;
		Py_INCREF(static_cast<PyObject*>(m_obj));
	}
}

PyRef& PyRef::operator=(const PyRef& other)
{
	if (this == &other || m_obj == other.m_obj)
		return *this;

	py::gil_scoped_acquire gil;
	Py_XINCREF(static_cast<PyObject*>(other.m_obj));
	Py_XDECREF(static_cast<PyObject*>(m_obj));
	m_obj = other.m_obj;

	return *this;
}

PyRef::~PyRef()
{
	reset();
}

void PyRef::reset()
{
	if (!m_obj)
		return;

	py::gil_scoped_acquire gil;
	Py_DECREF(static_cast<PyObject*>(m_obj));
	m_obj = 0;
}
