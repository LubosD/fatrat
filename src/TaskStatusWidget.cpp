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

#include "TaskStatusWidget.h"
#include "TaskGui.h"
#include "fatrat.h"

#include "plugins/Task.h"
#include "plugins/TaskManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QProgressBar>
#include <QTimer>

static const int ShowResultMs = 5000;

TaskStatusWidget::TaskStatusWidget(Task* task, TaskGui* gui, QWidget* parent)
	: QWidget(parent), m_task(task), m_taskId(task->id()), m_gui(gui)
{
	QHBoxLayout* layout = new QHBoxLayout(this);

	layout->setContentsMargins(0, 0, 0, 0);
	// The text is a label of its own: styles draw no text on a busy bar.
	m_label = new QLabel(this);
	m_label->setMaximumWidth(300);
	layout->addWidget(m_label);
	m_bar = new QProgressBar(this);
	m_bar->setFixedWidth(120);
	layout->addWidget(m_bar);

	connect(TaskManager::instance(), &TaskManager::taskChanged, this, [this](Task* t) {
		if (t == m_task)
			refresh();
	});
	connect(TaskManager::instance(), &TaskManager::taskRemoved, this, [this](const QString& id) {
		if (id == m_taskId)
			dispose();
	});

	refresh();
}

void TaskStatusWidget::refresh()
{
	if (!m_task)
		return;

	QString title = m_task->title();
	QString status = m_task->status();
	int progress = m_task->progress();

	// An unknown progress shows as a busy bar.
	m_bar->setRange(0, progress < 0 ? 0 : 100);
	if (progress >= 0)
		m_bar->setValue(progress);

	switch (m_task->state())
	{
	case Task::Waiting:
		m_label->setText(tr("%1: needs an answer").arg(title));
		break;
	case Task::Done:
		m_bar->setRange(0, 100);
		m_bar->setValue(100);
		m_label->setText(tr("%1: %2").arg(title, m_task->message().isEmpty() ? tr("done") : m_task->message()));
		break;
	case Task::Failed:
	case Task::Cancelled:
		m_bar->setRange(0, 100);
		m_label->setText(tr("%1: %2").arg(title, m_task->message().isEmpty() ? tr("failed") : m_task->message()));
		break;
	case Task::Running:
		m_label->setText(status.isEmpty() ? title : tr("%1: %2").arg(title, status));
		break;
	}

	setToolTip(status.isEmpty() ? title : QString("%1\n%2").arg(title, status));

	if (m_task->isFinished() && !m_disposing)
	{
		m_disposing = true;
		QTimer::singleShot(ShowResultMs, this, &TaskStatusWidget::dispose);
	}
}

void TaskStatusWidget::mousePressEvent(QMouseEvent* event)
{
	if (!m_task)
		return;

	QMenu menu(this);
	QPointer<Task> task = m_task;

	menu.addAction(m_task->title())->setEnabled(false);
	if (!m_task->status().isEmpty())
		menu.addAction(m_task->status())->setEnabled(false);
	menu.addSeparator();

	if (m_task->prompt())
		menu.addAction(tr("Answer..."), this, [this, task] { if (task) m_gui->showPrompt(task); });
	if (!m_task->isFinished())
		menu.addAction(tr("Cancel"), this, [task] { if (task) TaskManager::instance()->cancel(task->id()); });

	menu.exec(event->globalPosition().toPoint());
}

// Called by the result timer and by taskRemoved, whichever comes first.
void TaskStatusWidget::dispose()
{
	if (m_disposed)
		return;
	m_disposed = true;
	removeStatusWidget(this);
	deleteLater();
}
