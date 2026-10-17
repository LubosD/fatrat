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

#include "tools/PythonFileSearch.h"

#include <QHeaderView>
#include <QPointer>
#include <QTimer>
#include <QtDebug>

#include "MainWindow.h"
#include "Settings.h"
#include "fatrat.h"
#include "python/PluginServices.h"
#include "python/PyPlugin.h"

namespace py = pybind11;

PythonFileSearch::PythonFileSearch(QWidget* parent)
	: QWidget(parent), m_nPending(0)
{
	setupUi(this);
	addSearchEngines();

	connect(pushSearch, SIGNAL(clicked()), this, SLOT(searchClicked()));
	connect(pushDownload, SIGNAL(clicked()), this, SLOT(downloadClicked()));

	QHeaderView* header = treeResults->header();

	header->resizeSection(0, 350);
	header->resizeSection(2, 200);

	QTimer::singleShot(100, this, SLOT(setSearchFocus()));
}

PythonFileSearch::~PythonFileSearch()
{
	py::gil_scoped_acquire gil;

	m_running.clear();
}

bool PythonFileSearch::available()
{
	return !PluginServices::searchPlugins().isEmpty();
}

void PythonFileSearch::setSearchFocus()
{
	lineExpr->setFocus(Qt::OtherFocusReason);
}

void PythonFileSearch::addSearchEngines()
{
	QStringList last = getSettingsValue(QLatin1String("python/search_servers")).toStringList();

	foreach (const PluginServices::Entry& entry, PluginServices::searchPlugins())
	{
		QListWidgetItem* item = new QListWidgetItem(entry.name, listEngines);

		item->setData(Qt::UserRole, entry.identifier);
		item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		item->setCheckState(last.contains(entry.identifier) ? Qt::Checked : Qt::Unchecked);

		listEngines->addItem(item);
	}
}

void PythonFileSearch::enableControls(bool enable)
{
	listEngines->setEnabled(enable);
	pushSearch->setEnabled(enable);
	pushDownload->setEnabled(enable);
	lineExpr->setEnabled(enable);
	treeResults->setEnabled(enable);
	progressBar->setEnabled(!enable);
}

void PythonFileSearch::searchClicked()
{
	QString query = lineExpr->text().trimmed();

	treeResults->clear();
	progressBar->setValue(0);
	progressBar->setMaximum(0);

	if (query.isEmpty())
		return;

	enableControls(false);

	QStringList selected;
	QList<PluginServices::Entry> engines = PluginServices::searchPlugins();

	m_running.clear();
	m_nPending = 0;

	py::gil_scoped_acquire gil;

	for (int i = 0; i < listEngines->count() && i < engines.size(); i++)
	{
		if (listEngines->item(i)->checkState() != Qt::Checked)
			continue;

		const PluginServices::Entry& engine = engines[i];
		QString engineName = engine.name;

		selected << engine.identifier;

		try
		{
			py::object instance = pyFromRef(engine.cls)();
			PyPlugin* plugin = instance.cast<PyPlugin*>();

			plugin->setPluginName(engineName);
			m_running << pyToRef(instance);

			py::object coroutine = instance.attr("search")(query.toStdString());

			// The user may close the tool while a site is still thinking.
			QPointer<PythonFileSearch> guard(this);

			pySpawnTask(coroutine, [guard, engineName](py::object result, py::object error)
			{
				if (!guard)
					return;

				QList<SearchResult> results;

				if (!error.is_none())
				{
					qDebug() << "Search plugin" << engineName << "failed:"
						 << QString::fromStdString(py::str(error).cast<std::string>());
				}
				else if (!result.is_none())
				{
					for (py::handle item : result)
					{
						SearchResult found;

						found.name = QString::fromStdString(py::str(item.attr("name")).cast<std::string>());
						found.url = QString::fromStdString(py::str(item.attr("url")).cast<std::string>());
						found.extra = QString::fromStdString(py::str(item.attr("extra")).cast<std::string>());
						found.fileSize = item.attr("size").cast<qint64>();

						results << found;
					}
				}

				guard->addResults(engineName, results);
			});

			m_nPending++;
		}
		catch (const py::error_already_set& e)
		{
			qDebug() << "Search plugin" << engineName << "could not start:" << pyErrorText(e);
		}
	}

	setSettingsValue(QLatin1String("python/search_servers"), selected);
	progressBar->setMaximum(m_nPending);

	if (!m_nPending)
		searchFinished();
}

void PythonFileSearch::addResults(const QString& engineName, const QList<SearchResult>& results)
{
	foreach (const SearchResult& result, results)
	{
		QTreeWidgetItem* item = new QTreeWidgetItem(treeResults);

		item->setText(0, result.name);
		item->setText(1, formatSize(result.fileSize));
		item->setData(0, Qt::UserRole, result.url);
		item->setText(2, engineName);
		item->setText(3, result.extra);

		treeResults->addTopLevelItem(item);
	}

	m_nPending--;
	progressBar->setValue(progressBar->maximum() - m_nPending);

	if (m_nPending <= 0)
		searchFinished();
}

void PythonFileSearch::searchFinished()
{
	enableControls(true);
	progressBar->setMaximum(1);
	progressBar->setValue(1);

	py::gil_scoped_acquire gil;
	m_running.clear();
}

void PythonFileSearch::downloadClicked()
{
	QList<QTreeWidgetItem*> selection = treeResults->selectedItems();
	QString urls;

	foreach (QTreeWidgetItem* item, selection)
		urls += item->data(0, Qt::UserRole).toString() + "\n";

	if (!urls.isEmpty())
	{
		MainWindow* window = (MainWindow*) getMainWindow();
		window->addTransfer(urls, Transfer::Download);
	}
}
