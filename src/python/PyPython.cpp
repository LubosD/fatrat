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

#include "PyPython.h"

#include <QtDebug>

namespace py = pybind11;

QString pyErrorText(const py::error_already_set& e)
{
	// what() already carries "Type: message" plus the traceback when the
	// exception came with one.
	return QString::fromUtf8(e.what());
}

py::object pyFromVariant(const QVariant& v)
{
	switch ((QMetaType::Type) v.userType())
	{
	case QMetaType::UnknownType:
		return py::none();
	case QMetaType::Bool:
		return py::bool_(v.toBool());
	case QMetaType::Int:
	case QMetaType::UInt:
	case QMetaType::Long:
	case QMetaType::LongLong:
	case QMetaType::ULongLong:
		return py::int_(v.toLongLong());
	case QMetaType::Double:
	case QMetaType::Float:
		return py::float_(v.toDouble());
	case QMetaType::QByteArray:
	{
		QByteArray ba = v.toByteArray();
		return py::bytes(ba.constData(), ba.size());
	}
	case QMetaType::QStringList:
	{
		py::list list;
		foreach (QString s, v.toStringList())
			list.append(py::str(s.toStdString()));
		return list;
	}
	default:
		return py::str(v.toString().toStdString());
	}
}

QVariant pyToVariant(py::handle h)
{
	if (h.is_none())
		return QVariant();
	if (py::isinstance<py::bool_>(h))
		return QVariant(h.cast<bool>());
	if (py::isinstance<py::int_>(h))
		return QVariant(qlonglong(h.cast<long long>()));
	if (py::isinstance<py::float_>(h))
		return QVariant(h.cast<double>());
	if (py::isinstance<py::bytes>(h))
	{
		std::string s = h.cast<std::string>();
		return QVariant(QByteArray(s.data(), s.size()));
	}
	if (py::isinstance<py::str>(h))
		return QVariant(QString::fromStdString(h.cast<std::string>()));
	if (py::isinstance<py::list>(h) || py::isinstance<py::tuple>(h))
	{
		QVariantList list;
		for (py::handle item : h)
			list << pyToVariant(item);
		return list;
	}
	if (py::isinstance<py::dict>(h))
	{
		QVariantMap map;
		for (std::pair<py::handle, py::handle> item : py::reinterpret_borrow<py::dict>(h))
			map[QString::fromStdString(py::str(item.first).cast<std::string>())] = pyToVariant(item.second);
		return map;
	}

	// Anything else: let the plugin author see what FatRat made of it.
	return QVariant(QString::fromStdString(py::str(h).cast<std::string>()));
}

PyRef pyToRef(py::handle h)
{
	return PyRef(h.ptr());
}

py::object pyFromRef(const PyRef& ref)
{
	if (ref.isNull())
		return py::none();
	return py::reinterpret_borrow<py::object>(static_cast<PyObject*>(ref.get()));
}

py::module_ pyModuleFatrat()
{
	return py::module_::import("fatrat");
}

py::module_ pyModuleAsync()
{
	return py::module_::import("fatrat._async");
}

py::module_ pyModuleHttp()
{
	return py::module_::import("fatrat._http");
}

py::module_ pyModuleErrors()
{
	return py::module_::import("fatrat.errors");
}

py::object pyMakeFuture(std::function<void()> onCancel)
{
	py::object cls = pyModuleAsync().attr("Future");

	if (!onCancel)
		return cls();

	return cls(py::cpp_function([onCancel]() { onCancel(); }));
}

void pyResolveFuture(const py::object& future, py::object value)
{
	if (future.is_none())
		return;
	future.attr("set_result")(value);
}

void pyFailFuture(const py::object& future, const char* exception, const QString& message)
{
	if (future.is_none())
		return;

	py::object cls = pyModuleErrors().attr(exception);
	future.attr("set_exception")(cls(message.toStdString()));
}

py::object pySpawnTask(py::object coroutine, std::function<void(py::object, py::object)> onDone)
{
	py::object task = pyModuleAsync().attr("Task")(coroutine,
		py::cpp_function([onDone](py::object result, py::object error)
		{
			onDone(result, error);
		}));

	task.attr("start")();

	return task;
}
