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

#ifndef TASKPUSH_H
#define TASKPUSH_H
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <functional>

class Task;

/* Turns TaskManager's signals into /websocket messages:
 *   {"type": "task", "task": {...}} and {"type": "task-removed", "id": "..."}.
 *
 * A task working through a big archive changes its progress constantly, so a
 * change of only progress and status goes out at most twice a second per
 * task. Anything else -- a question, the end -- goes out at once, along with
 * whatever was being held back.
 */
class TaskPush : public QObject
{
Q_OBJECT
public:
	static const int ProgressIntervalMs = 500;

	explicit TaskPush(std::function<void(const QJsonObject&)> send, QObject* parent = nullptr);
private:
	struct Sent
	{
		QJsonObject json;
		qint64 at = 0;
		bool pending = false;
	};

	void changed(Task* task);
	void send(const QString& id, const QJsonObject& json);
	void removed(const QString& id);

	std::function<void(const QJsonObject&)> m_send;
	QHash<QString, Sent> m_sent;
	QElapsedTimer m_clock;
};

#endif
