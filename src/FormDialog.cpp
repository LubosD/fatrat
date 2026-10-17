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

#include "FormDialog.h"
#include "FormWidgets.h"
#include "fatrat.h"

#include "plugins/Task.h"
#include "plugins/TaskManager.h"

#include <QDialogButtonBox>
#include <QJsonArray>
#include <QLabel>
#include <QVBoxLayout>

FormDialog::FormDialog(Task* task, QWidget* parent)
	: QDialog(parent), m_task(task), m_taskId(task->id())
{
	std::optional<Task::Prompt> prompt = task->prompt();
	QVBoxLayout* layout = new QVBoxLayout(this);

	setAttribute(Qt::WA_DeleteOnClose);

	if (!prompt)
	{
		// Answered before we got here.
		m_done = true;
		QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
		return;
	}

	m_promptId = prompt->id;
	m_expiresAt = prompt->expiresAt;

	QString title = prompt->form.value("title").toString();
	setWindowTitle(title.isEmpty() ? task->title() : title);

	QString description = prompt->form.value("description").toString();
	if (!description.isEmpty())
	{
		QLabel* label = new QLabel(description, this);
		label->setWordWrap(true);
		layout->addWidget(label);
	}

	QWidget* fields = new QWidget(this);
	m_widgets.reset(new FormWidgets(prompt->form.value("fields").toArray(), QJsonObject(), fields));
	layout->addWidget(fields);

	m_error = new QLabel(this);
	m_error->setStyleSheet("color: red");
	m_error->setWordWrap(true);
	m_error->hide();
	layout->addWidget(m_error);

	m_countdown = new QLabel(this);
	layout->addWidget(m_countdown);

	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, &FormDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &FormDialog::reject);
	layout->addWidget(buttons);

	connect(TaskManager::instance(), &TaskManager::promptClosed, this, [this](Task*, const QString& id) {
		if (id == m_promptId)
			closeQuietly();
	});
	connect(TaskManager::instance(), &TaskManager::taskRemoved, this, [this](const QString& id) {
		if (id == m_taskId)
			closeQuietly();
	});

	connect(&m_timer, &QTimer::timeout, this, &FormDialog::updateCountdown);
	m_timer.start(1000);
	updateCountdown();
}

FormDialog::~FormDialog() = default;

void FormDialog::updateCountdown()
{
	qint64 secs = qMax<qint64>(0, QDateTime::currentDateTimeUtc().secsTo(m_expiresAt));
	m_countdown->setText(tr("Without an answer, this closes in %1.").arg(formatTime(secs)));
}

void FormDialog::closeQuietly()
{
	m_done = true;
	close();
}

void FormDialog::accept()
{
	if (m_done || !m_task)
	{
		closeQuietly();
		return;
	}

	QString error;
	// Answering closes the prompt, which closes this dialog through promptClosed.
	m_done = true;
	if (m_task->answer(m_promptId, m_widgets->values(), &error) == Task::Invalid)
	{
		m_done = false;
		m_error->setText(error);
		m_error->show();
		return;
	}
	closeQuietly();
}

void FormDialog::reject()
{
	if (!m_done && m_task)
	{
		m_done = true;
		m_task->dismissPrompt(m_promptId);
	}
	QDialog::reject();
}
