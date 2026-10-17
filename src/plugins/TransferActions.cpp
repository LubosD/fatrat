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

#include "TransferActions.h"
#include "Logger.h"
#include "Task.h"
#include "TaskManager.h"

#include <QCoreApplication>
#include <QObject>
#include <QThread>
#include <exception>

static QList<TransferActions::Entry> s_actions;
static QString s_loadingPlugin;

static QString currentPlugin()
{
	return s_loadingPlugin.isEmpty() ? QString("fatrat") : s_loadingPlugin;
}

TaskHost::TaskHost(const QString& plugin, const QString& transferUuid, const QString& transferName)
	: m_plugin(plugin), m_transferUuid(transferUuid), m_transferName(transferName)
{
}

Task* TaskHost::createTask(const QString& title)
{
	Task* task = TaskManager::instance()->create(m_plugin, title, m_transferUuid, m_transferName);
	m_tasks << task;
	return task;
}

void registerTransferAction(const TransferAction& action)
{
	// The registry is unsynchronised and meant for initPlugin() on the main thread.
	if (QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread())
	{
		// Not currentPlugin(): that reads s_loadingPlugin, which belongs to the main thread.
		Logger::global()->enterLogMessage(QStringLiteral("fatrat"),
			QObject::tr("A plugin registered an action from another thread; ignored"));
		return;
	}

	for (const TransferActions::Entry& e : s_actions)
	{
		if (e.action.id == action.id)
		{
			Logger::global()->enterLogMessage(currentPlugin(),
				QObject::tr("Ignored a second action called %1").arg(action.id));
			return;
		}
	}
	s_actions << TransferActions::Entry{action, currentPlugin()};
}

bool TransferActions::applies(const Entry& entry, Transfer* t)
{
	if (!entry.action.appliesTo)
		return true;

	QString message;

	try
	{
		return entry.action.appliesTo(t);
	}
	catch (const std::exception& e)
	{
		message = QString::fromUtf8(e.what());
	}
	catch (...)
	{
		message = QObject::tr("unknown error");
	}

	Logger::global()->enterLogMessage(entry.plugin, QObject::tr("The action %1 failed to say whether it applies: %2")
		.arg(entry.action.id, message));
	return false;
}

QList<TransferActions::Entry> TransferActions::applicable(Transfer* t)
{
	QList<Entry> rv;

	for (const Entry& e : s_actions)
	{
		if (applies(e, t))
			rv << e;
	}
	return rv;
}

std::optional<TransferActions::Entry> TransferActions::find(const QString& id)
{
	for (const Entry& e : s_actions)
	{
		if (e.action.id == id)
			return e;
	}
	return std::nullopt;
}

Task* TransferActions::trigger(const Entry& entry, Transfer* t, const QString& transferUuid, const QString& transferName)
{
	TaskHost host(entry.plugin, transferUuid, transferName);
	QString message;

	try
	{
		if (entry.action.trigger)
			entry.action.trigger(t, host);
		return host.createdTask();
	}
	catch (const std::exception& e)
	{
		message = QString::fromUtf8(e.what());
	}
	catch (...)
	{
		message = QObject::tr("unknown error");
	}

	// Finishing a task as failed logs the failure itself, so only log when
	// there was no task to do it.
	bool logged = false;
	for (Task* task : host.tasks())
	{
		if (!task->isFinished())
		{
			task->finish(false, message);
			logged = true;
		}
	}

	if (!logged)
	{
		Logger::global()->enterLogMessage(entry.plugin, QObject::tr("The action %1 failed: %2")
			.arg(entry.action.id, message));
	}

	throw PluginError{entry.plugin, message};
}

void TransferActions::setLoadingPlugin(const QString& name)
{
	s_loadingPlugin = name;
}

void TransferActions::clear()
{
	s_actions.clear();
	s_loadingPlugin.clear();
}

void TransferActions::removePlugin(const QString& name)
{
	for (int i = s_actions.size() - 1; i >= 0; i--)
	{
		if (s_actions[i].plugin == name)
			s_actions.removeAt(i);
	}
}
