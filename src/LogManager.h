/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef LOGMANAGER_H
#define LOGMANAGER_H
#include <QObject>
#include <QTextEdit>
#include "Transfer.h"

class LogManager : public QObject
{
Q_OBJECT
public:
	LogManager(QObject* parent, QTextEdit* widgetT, QTextEdit* widgetG)
	: QObject(parent), m_text(widgetT), m_textG(widgetG), m_last(0)
	{
		widgetG->setPlainText(Logger::global()->logContents());
		connect(Logger::global(), SIGNAL(logMessage(QString)), widgetG, SLOT(append(const QString&)));
	}
	void setLogSource(Transfer* t)
	{
		if(t == m_last)
			return;
		
		if(m_last != 0)
		{
			disconnect(m_last, SIGNAL(logMessage(QString)), m_text, SLOT(append(const QString&)));
			disconnect(m_last, SIGNAL(logMessage(QString)), this, SLOT(onLogMessage()));
			disconnect(m_last, SIGNAL(destroyed()), this, SLOT(onDeleteSource()));
		}
		m_last = t;
		
		if(t != 0)
		{
			m_text->setEnabled(true);
			m_text->setPlainText(t->logContents());
			connect(t, SIGNAL(logMessage(QString)), m_text, SLOT(append(const QString&)));
			connect(t, SIGNAL(logMessage(QString)), this, SLOT(onLogMessage()));
			connect(t, SIGNAL(destroyed()), this, SLOT(onDeleteSource()));
		}
		else
		{
			m_text->clear();
			m_text->setEnabled(false);
		}
	}
public slots:
	void onDeleteSource()
	{
		m_last = 0;
	}
	void onLogMessage()
	{
		if (m_text->toPlainText().size() > 2*1024*1024)
			m_text->setPlainText(m_last->logContents());
	}
private:
	QTextEdit *m_text, *m_textG;
	Transfer* m_last;
};

#endif
