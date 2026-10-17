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

#ifndef PLUGINTASK_H
#define PLUGINTASK_H
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>
#include <optional>

/* Something a plugin does on the user's behalf, visible in every front end.
 *
 * A task reports progress and a status line, and can stop to ask the user a
 * question: a form, see FormValidator.h. Every front end shows the question
 * at once and the first answer wins. Every question has a timeout; when it
 * runs out, or the user dismisses the question, or the task is cancelled,
 * the plugin gets std::nullopt and decides what that means.
 *
 * Tasks are created through TaskHost (TransferAction.h), owned by TaskManager
 * and live on the main thread. The setters, finish() and askBlocking() may be
 * called from any thread; ask() and the front ends' side only from the main
 * thread.
 */
class Task : public QObject
{
Q_OBJECT
public:
	enum State { Running, Waiting, Done, Failed, Cancelled };
	enum AnswerResult { Answered, AlreadyAnswered, Expired, UnknownPrompt, Invalid };
	typedef std::function<void(std::optional<QJsonObject>)> AnswerCallback;

	struct Prompt
	{
		QString id;
		QJsonObject form;
		QDateTime expiresAt;
	};

	// Every timeout given to ask()/askBlocking() is clamped to these.
	static constexpr int MinTimeoutMs = 1000;
	static constexpr int MaxTimeoutMs = 24 * 60 * 60 * 1000;

	QString id() const { return m_id; }
	QString plugin() const { return m_plugin; }
	QString title() const { return m_title; }
	QString transferUuid() const { return m_transferUuid; }
	QString transferName() const { return m_transferName; }

	State state() const;
	int progress() const;
	QString status() const;
	QString message() const;
	std::optional<Prompt> prompt() const;
	bool isFinished() const;
	bool cancelRequested() const;

	// Any thread
	void setProgress(int percent); // 0..100, or -1 for "unknown"
	void setStatus(const QString& text);
	// A second call is ignored. An open question is closed (its callback gets std::nullopt).
	void finish(bool ok, const QString& message = QString());

	// Main thread only
	void ask(const QJsonObject& form, int timeoutMs, AnswerCallback callback);
	// Worker threads only: blocks until ask()'s callback would have run. Never call it
	// from a thread the main thread may be waiting for.
	std::optional<QJsonObject> askBlocking(const QJsonObject& form, int timeoutMs);

	// The front ends' side, main thread only
	AnswerResult answer(const QString& promptId, const QJsonObject& values, QString* error = nullptr);
	AnswerResult dismissPrompt(const QString& promptId);
	void requestCancel();
signals:
	void changed();
	void cancelled();
	void promptOpened();
	void promptClosed(const QString& promptId);
private:
	friend class TaskManager;
	Task(const QString& plugin, const QString& title, const QString& transferUuid,
	     const QString& transferName, QObject* parent);

	enum CloseReason { ClosedAnswered, ClosedExpired, ClosedDismissed };
	void closePrompt(CloseReason reason, std::optional<QJsonObject> result);
	void runCallback(const AnswerCallback& callback, std::optional<QJsonObject> result);
	AnswerResult closedResult(const QString& promptId) const; // m_mutex held

	const QString m_id, m_plugin, m_title, m_transferUuid, m_transferName;

	mutable QMutex m_mutex; // guards everything below except m_expiry
	State m_state = Running; // never Waiting: that is derived from m_prompt
	int m_progress = -1;
	QString m_status, m_message;
	bool m_cancelRequested = false;
	std::optional<Prompt> m_prompt;
	AnswerCallback m_callback;
	QHash<QString, CloseReason> m_closedPrompts;

	QTimer m_expiry; // main thread
};

#endif
