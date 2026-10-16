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

// Python headers before Qt; see src/python/PyRef.h.
#include "python/PyPython.h"

#include "engines/PythonAccountStatusWidget.h"

#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QtDebug>

#include "python/PluginServices.h"
#include "python/PyPlugin.h"

namespace py = pybind11;

bool PythonAccountStatusWidget::available()
{
	return !PluginServices::accountStatusPlugins().isEmpty();
}

PythonAccountStatusWidget::PythonAccountStatusWidget(QWidget* parent)
	: QFrame(parent)
{
	setupUi(this);

	QPalette palette = treeWidget->palette();

	palette.setColor(QPalette::Base, palette.color(QPalette::Window));
	treeWidget->setPalette(palette);
	treeWidget->header()->resizeSection(0, 25);

	py::gil_scoped_acquire gil;

	foreach (const PluginServices::Entry& entry, PluginServices::accountStatusPlugins())
	{
		QString name = entry.name;

		try
		{
			py::object instance = pyFromRef(entry.cls)();
			PyPlugin* plugin = instance.cast<PyPlugin*>();

			plugin->setPluginName(name);
			m_running << pyToRef(instance);

			QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget);
			QLabel* label = new QLabel(treeWidget);

			label->setText(QString("<b>%1:</b> %2").arg(name).arg(tr("Requesting...")));
			item->setIcon(0, QIcon(":/states/waiting.png"));
			treeWidget->setItemWidget(item, 1, label);

			m_items[name] = item;

			QPointer<PythonAccountStatusWidget> guard(this);

			pySpawnTask(instance.attr("account_status")(),
				    [guard, name](py::object result, py::object error)
			{
				if (!guard)
					return;

				if (!error.is_none() || result.is_none())
				{
					QString message = error.is_none()
						? tr("No answer")
						: QString::fromStdString(py::str(error).cast<std::string>());

					guard->accountStatusReceived(name, 3 /* error */, message);
					return;
				}

				guard->accountStatusReceived(name,
					result.attr("state").attr("value").cast<int>(),
					QString::fromStdString(py::str(result.attr("balance")).cast<std::string>()));
			});
		}
		catch (const py::error_already_set& e)
		{
			qDebug() << "Account status plugin" << name << "could not start:" << pyErrorText(e);
		}
	}
}

PythonAccountStatusWidget::~PythonAccountStatusWidget()
{
	py::gil_scoped_acquire gil;

	m_running.clear();
}

void PythonAccountStatusWidget::accountStatusReceived(const QString& name, int state, const QString& balance)
{
	QTreeWidgetItem* item = m_items.value(name);

	if (!item)
		return;

	QLabel* label = static_cast<QLabel*>(treeWidget->itemWidget(item, 1));

	if (label)
		label->setText(QString("<b>%1:</b> %2").arg(name).arg(balance));

	const char* icon;

	switch (state)
	{
	case 0:
		icon = ":/acc_state/green.png";
		break;
	case 1:
		icon = ":/acc_state/yellow.png";
		break;
	case 2:
		icon = ":/acc_state/red.png";
		break;
	default:
		icon = ":/states/failed.png";
		break;
	}

	item->setIcon(0, QIcon(icon));
}
