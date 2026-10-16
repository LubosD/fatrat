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

#include "PyBindings.h"

#include <QtDebug>

#include "Transfer.h"
#include "engines/PythonDownload.h"
#include "engines/StaticTransferMessage.h"

namespace py = pybind11;

static StaticTransferMessage<Transfer>* transferOf(PyPlugin& plugin, const char* what)
{
	StaticTransferMessage<Transfer>* transfer = plugin.transfer();

	if (!transfer)
		throw std::runtime_error(std::string(what) + "() needs a transfer; this plugin has none");

	return transfer;
}

static PythonDownload* downloadOf(PyPlugin& plugin, const char* what)
{
	PythonDownload* download = dynamic_cast<PythonDownload*>(plugin.transfer());

	if (!download)
		throw std::runtime_error(std::string(what) + "() is only available to a download plugin");

	return download;
}

void registerTransferBindings(PyPluginClass& cls)
{
	cls.def("set_message", [](PyPlugin& self, py::object text)
	{
		transferOf(self, "set_message")->setMessage(
			QString::fromStdString(py::str(text).cast<std::string>()));
	}, py::arg("text"), "Set the one-line status shown in the transfer list.");

	cls.def("set_state", [](PyPlugin& self, int state)
	{
		transferOf(self, "set_state")->setState(Transfer::State(state));
	}, py::arg("state"));

	cls.def("fail", [](PyPlugin& self, py::object message)
	{
		QString text = QString::fromStdString(py::str(message).cast<std::string>());
		StaticTransferMessage<Transfer>* transfer = transferOf(self, "fail");

		transfer->setMessage(text);
		transfer->enterLogMessage(text);
		transfer->setState(Transfer::Failed);
	}, py::arg("message"), "Fail this transfer with a message the user will read.");

	// self.vars[...] -- values that survive a restart with the transfer.
	cls.def("_var_get", [](PyPlugin& self, const std::string& key) -> py::object
	{
		QVariant value = downloadOf(self, "vars")->getPersistentVariable(QString::fromStdString(key));

		return value.isValid() ? pyFromVariant(value) : py::none();
	}, py::arg("key"));

	cls.def("_var_set", [](PyPlugin& self, const std::string& key, py::handle value)
	{
		downloadOf(self, "vars")->setPersistentVariable(QString::fromStdString(key), pyToVariant(value));
	}, py::arg("key"), py::arg("value"));

	cls.def("_var_names", [](PyPlugin& self)
	{
		py::list names;

		foreach (QString name, downloadOf(self, "vars")->persistentVariableNames())
			names.append(py::str(name.toStdString()));

		return names;
	});

	cls.def("report_file_name", [](PyPlugin& self, py::object name)
	{
		downloadOf(self, "report_file_name")->setName(
			QString::fromStdString(py::str(name).cast<std::string>()));
	}, py::arg("name"), "Name the transfer before its download starts.");

	cls.def("start_download", [](PyPlugin& self, py::object target)
	{
		PythonDownload* download = downloadOf(self, "start_download");
		QString url, referrer, userAgent, postData, fileName;

		if (py::isinstance<py::str>(target))
		{
			url = QString::fromStdString(target.cast<std::string>());
		}
		else
		{
			// A fatrat.DownloadUrl (or anything shaped like one).
			url = QString::fromStdString(py::str(target.attr("url")).cast<std::string>());
			referrer = QString::fromStdString(py::str(target.attr("referrer")).cast<std::string>());
			userAgent = QString::fromStdString(py::str(target.attr("user_agent")).cast<std::string>());
			postData = QString::fromStdString(py::str(target.attr("post_data")).cast<std::string>());

			py::object name = target.attr("file_name");
			if (!name.is_none())
				fileName = QString::fromStdString(py::str(name).cast<std::string>());
		}

		if (url.isEmpty())
			throw std::runtime_error("start_download() needs a URL");

		download->startDownload(url, referrer, userAgent, postData, fileName);
	}, py::arg("target"), "Hand the resolved link back to FatRat and finish.");
}
