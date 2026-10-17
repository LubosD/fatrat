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

#ifndef FORMDIALOG_H
#define FORMDIALOG_H
#include <QDateTime>
#include <QDialog>
#include <QPointer>
#include <QTimer>
#include <memory>

class FormWidgets;
class QLabel;
class Task;

/* A plugin's question (Task::ask) on the desktop.
 *
 * Not modal: a plugin waiting for an answer must not block the main window.
 * It closes itself when the question is answered elsewhere (the web UI),
 * expires, or its task goes away, and deletes itself on close. Closing it
 * without answering dismisses the question.
 */
class FormDialog : public QDialog
{
Q_OBJECT
public:
	explicit FormDialog(Task* task, QWidget* parent = nullptr);
	~FormDialog();

	QString promptId() const { return m_promptId; }
public slots:
	void accept() override;
	void reject() override;
private:
	void updateCountdown();
	void closeQuietly();

	QPointer<Task> m_task;
	QString m_taskId, m_promptId;
	QDateTime m_expiresAt;
	std::unique_ptr<FormWidgets> m_widgets;
	QLabel* m_error = nullptr;
	QLabel* m_countdown = nullptr;
	QTimer m_timer;
	bool m_done = false; // answered, dismissed, or closed elsewhere
};

#endif
