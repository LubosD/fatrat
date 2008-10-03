/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H
#include <typeinfo>

class MyApplication : public QApplication
{
public:
	MyApplication(int& argc, char** argv, bool gui)
		: QApplication(argc, argv, gui)
	{
	}

	virtual bool notify(QObject* receiver, QEvent* e)
	{
		try
		{
			QApplication::notify(receiver, e);
		}
		catch(const std::exception& e)
		{
			reportException(QString::fromUtf8(e.what()), typeid(e).name());
		}
		catch(const RuntimeException& e)
		{
			reportException(e.what(), typeid(e).name());
		}
		
		return false;
	}
private:
	static void reportException(QString text, QString type)
	{
		QMessageBox::critical(0, tr("Unhandled exception"),
				tr("The main handler has caught the following exception."
				" This is a bug and should be reported as such.\n\n"
				"Type of exception: %1\nMessage: %2").arg(type).arg(text));
	}
};

#endif
