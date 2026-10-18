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

#ifndef TASKSTATUSWIDGET_H
#define TASKSTATUSWIDGET_H
#include <QPointer>
#include <QWidget>

class QLabel;
class QProgressBar;
class Task;
class TaskGui;

/* One plugin task in the main window's status bar. A click opens a menu to
 * answer its question or cancel it. Once the task has finished it shows the
 * result for a few seconds and goes away. */
class TaskStatusWidget : public QWidget
{
Q_OBJECT
public:
	TaskStatusWidget(Task* task, TaskGui* gui, QWidget* parent = nullptr);
protected:
	void mousePressEvent(QMouseEvent* event) override;
private:
	void refresh();
	void dispose();

	QPointer<Task> m_task;
	QString m_taskId;
	TaskGui* m_gui;
	QLabel* m_label;
	QProgressBar* m_bar;
	bool m_disposing = false; // the result timer is running
	bool m_disposed = false;
};

#endif
