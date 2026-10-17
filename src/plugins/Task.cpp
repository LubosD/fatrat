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

#include "Task.h"
#include "FormValidator.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QThread>
#include <QUuid>
#include <QWaitCondition>
#include <QtDebug>
#include <exception>
#include <memory>

static bool onMainThread()
{
	return QThread::currentThread() == QCoreApplication::instance()->thread();
}

Task::Task(const QString& plugin, const QString& title, const QString& transferUuid,
	   const QString& transferName, QObject* parent)
	: QObject(parent), m_id(QUuid::createUuid().toString(QUuid::WithoutBraces)), m_plugin(plugin),
	  m_title(title), m_transferUuid(transferUuid), m_transferName(transferName)
{
	m_expiry.setSingleShot(true);
	connect(&m_expiry, &QTimer::timeout, this, [this] { closePrompt(ClosedExpired, std::nullopt); });
}

Task::~Task()
{
	// Wakes a thread still blocked in askBlocking().
	closePrompt(ClosedDismissed, std::nullopt);
}

Task::State Task::state() const
{
	QMutexLocker l(&m_mutex);
	return (m_state == Running && m_prompt) ? Waiting : m_state;
}

int Task::progress() const
{
	QMutexLocker l(&m_mutex);
	return m_progress;
}

QString Task::status() const
{
	QMutexLocker l(&m_mutex);
	return m_status;
}

QString Task::message() const
{
	QMutexLocker l(&m_mutex);
	return m_message;
}

std::optional<Task::Prompt> Task::prompt() const
{
	QMutexLocker l(&m_mutex);
	return m_prompt;
}

bool Task::isFinished() const
{
	QMutexLocker l(&m_mutex);
	return m_state != Running;
}

bool Task::cancelRequested() const
{
	QMutexLocker l(&m_mutex);
	return m_cancelRequested;
}

void Task::setProgress(int percent)
{
	{
		QMutexLocker l(&m_mutex);
		percent = qBound(-1, percent, 100);
		if (m_state != Running || m_progress == percent)
			return;
		m_progress = percent;
	}
	emit changed();
}

void Task::setStatus(const QString& text)
{
	{
		QMutexLocker l(&m_mutex);
		if (m_state != Running || m_status == text)
			return;
		m_status = text;
	}
	emit changed();
}

void Task::finish(bool ok, const QString& message)
{
	{
		QMutexLocker l(&m_mutex);
		if (m_state != Running)
		{
			qWarning() << "Task::finish() called twice for" << m_title << "by" << m_plugin;
			return;
		}
		m_state = ok ? Done : (m_cancelRequested ? Cancelled : Failed);
		m_message = message;
		if (ok)
			m_progress = 100;
	}

	// Headless, nobody watches the task list; the log is where a failure stays visible.
	if (!ok)
		Logger::global()->enterLogMessage(m_plugin, tr("%1 failed: %2").arg(m_title, message));

	// An open question has nobody left to want its answer.
	QMetaObject::invokeMethod(this, [this] { closePrompt(ClosedDismissed, std::nullopt); });
	emit changed();
}

void Task::ask(const QJsonObject& form, int timeoutMs, AnswerCallback callback)
{
	if (!onMainThread())
	{
		// The state below belongs to the main thread; hop over instead of racing.
		QMetaObject::invokeMethod(this, [=] { ask(form, timeoutMs, callback); }, Qt::QueuedConnection);
		return;
	}

	QString refusal = FormValidator::checkForm(form);
	timeoutMs = qBound(MinTimeoutMs, timeoutMs, MaxTimeoutMs);

	{
		QMutexLocker l(&m_mutex);

		if (!refusal.isEmpty())
			refusal = tr("asked with an invalid form: %1").arg(refusal);
		else if (m_prompt)
			refusal = tr("asked a second question while one was open");
		else if (m_state != Running || m_cancelRequested)
			refusal = tr("asked a question after its task ended");

		if (refusal.isEmpty())
		{
			m_prompt = Prompt{QUuid::createUuid().toString(QUuid::WithoutBraces), form,
				QDateTime::currentDateTimeUtc().addMSecs(timeoutMs)};
			m_callback = callback;
		}
	}

	if (!refusal.isEmpty())
	{
		// A plugin bug: say so, and give it the same "no answer" it must handle anyway.
		Logger::global()->enterLogMessage(m_plugin, refusal);
		runCallback(callback, std::nullopt);
		return;
	}

	m_expiry.start(timeoutMs);
	emit promptOpened();
	emit changed();
}

std::optional<QJsonObject> Task::askBlocking(const QJsonObject& form, int timeoutMs)
{
	if (onMainThread())
	{
		// Nobody could run the queued ask() while we wait: this would hang for good.
		Logger::global()->enterLogMessage(m_plugin, tr("asked a blocking question on the main thread"));
		return std::nullopt;
	}

	struct Wait
	{
		QMutex mutex;
		QWaitCondition cond;
		bool done = false;
		std::optional<QJsonObject> result;
	};
	auto wait = std::make_shared<Wait>();

	QMetaObject::invokeMethod(this, [this, form, timeoutMs, wait] {
		ask(form, timeoutMs, [wait](std::optional<QJsonObject> result) {
			QMutexLocker l(&wait->mutex);
			wait->result = result;
			wait->done = true;
			wait->cond.wakeAll();
		});
	}, Qt::QueuedConnection);

	QMutexLocker l(&wait->mutex);
	while (!wait->done)
		wait->cond.wait(&wait->mutex);
	return wait->result;
}

Task::AnswerResult Task::closedResult(const QString& promptId) const
{
	auto it = m_closedPrompts.constFind(promptId);

	if (it == m_closedPrompts.constEnd())
		return UnknownPrompt;
	return it.value() == ClosedExpired ? Expired : AlreadyAnswered;
}

Task::AnswerResult Task::answer(const QString& promptId, const QJsonObject& values, QString* error)
{
	QJsonObject normalized;

	{
		QMutexLocker l(&m_mutex);

		// finish() closes the prompt through a queued call that may not have run yet.
		if (m_state != Running)
			return AlreadyAnswered;
		if (!m_prompt || m_prompt->id != promptId)
			return closedResult(promptId);

		QString problem = FormValidator::checkAnswer(m_prompt->form, values, &normalized);
		if (!problem.isEmpty())
		{
			if (error)
				*error = problem;
			return Invalid;
		}
	}

	closePrompt(ClosedAnswered, normalized);
	return Answered;
}

Task::AnswerResult Task::dismissPrompt(const QString& promptId)
{
	{
		QMutexLocker l(&m_mutex);
		if (m_state != Running)
			return AlreadyAnswered;
		if (!m_prompt || m_prompt->id != promptId)
			return closedResult(promptId);
	}

	closePrompt(ClosedDismissed, std::nullopt);
	return Answered;
}

void Task::requestCancel()
{
	{
		QMutexLocker l(&m_mutex);
		if (m_state != Running || m_cancelRequested)
			return;
		m_cancelRequested = true;
	}

	emit cancelled();
	closePrompt(ClosedDismissed, std::nullopt);
	emit changed();
}

void Task::closePrompt(CloseReason reason, std::optional<QJsonObject> result)
{
	AnswerCallback callback;
	QString id;

	{
		QMutexLocker l(&m_mutex);
		if (!m_prompt)
			return;
		id = m_prompt->id;
		m_closedPrompts.insert(id, reason);
		m_prompt.reset();
		callback = std::move(m_callback);
		m_callback = nullptr;
	}

	m_expiry.stop();
	emit promptClosed(id);
	emit changed();
	runCallback(callback, result);
}

void Task::runCallback(const AnswerCallback& callback, std::optional<QJsonObject> result)
{
	if (!callback)
		return;

	try
	{
		callback(result);
	}
	catch (const std::exception& e)
	{
		if (!isFinished())
			finish(false, QString::fromUtf8(e.what()));
		else
			Logger::global()->enterLogMessage(m_plugin, QString::fromUtf8(e.what()));
	}
	catch (...)
	{
		if (!isFinished())
			finish(false, tr("unknown error"));
		else
			Logger::global()->enterLogMessage(m_plugin, tr("unknown error"));
	}
}
