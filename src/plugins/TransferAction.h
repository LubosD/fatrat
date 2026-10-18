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

#ifndef TRANSFERACTION_H
#define TRANSFERACTION_H
#include <QString>
#include <QList>
#include <functional>

class Task;
class Transfer;

/* How a plugin creates tasks, handed to TransferAction::trigger. Each task is
 * attributed to the plugin and the transfer the action was invoked on. */
class TaskHost
{
public:
	TaskHost(const QString& plugin, const QString& transferUuid, const QString& transferName);

	Task* createTask(const QString& title);
	// The first task created, if any. This is the one the front ends report
	// back to whoever triggered the action.
	Task* createdTask() const { return m_tasks.isEmpty() ? nullptr : m_tasks.first(); }
	// Every task created, in order.
	QList<Task*> tasks() const { return m_tasks; }
private:
	QString m_plugin, m_transferUuid, m_transferName;
	QList<Task*> m_tasks;
};

/* An entry in a transfer's context menu, in every front end.
 *
 * appliesTo and trigger run on the main thread with the transfer's queue
 * locked for reading. Keep appliesTo cheap -- the web UI calls it every time
 * a menu opens -- and modify no queues from either. Long work belongs in a
 * Task, on a thread of the plugin's own.
 */
struct TransferAction
{
	QString id;    // stable and namespaced, e.g. "unpack.unpack"
	QString label; // already translated
	QString icon;  // an icon theme name; may be empty
	std::function<bool(Transfer*)> appliesTo; // empty: always
	std::function<void(Transfer*, TaskHost&)> trigger;
};

// Call from initPlugin(). An action whose id is already registered is ignored.
void registerTransferAction(const TransferAction& action);

#endif
