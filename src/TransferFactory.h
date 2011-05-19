/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/


#ifndef TRANSFERFACTORY_H
#define TRANSFERFACTORY_H
#include <QObject>
#include "Transfer.h"
#include "RuntimeException.h"

class TransferFactory : public QObject
{
Q_OBJECT
public:
	TransferFactory();
	static TransferFactory* instance();

	// Create a Transfer instance in the correct thread
	Transfer* createInstance(const char* clsName);
	Transfer* createInstance(QString clsName);

	void setState(Transfer* t, Transfer::State state);

	// Init a Transfer in the correct thread
	void init(Transfer* t, QString source, QString target);
private slots:
	void createInstance(QString clsName, Transfer** t);
	void init(Transfer* t, QString source, QString target, RuntimeException* e, bool* eThrown);
	void setStateSlot(Transfer* t, Transfer::State state);
private:
	static TransferFactory* m_instance;
};

#endif // TRANSFERFACTORY_H
