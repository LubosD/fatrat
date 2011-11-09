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

#include "TransferFactory.h"
#include <QThread>
#include <QMetaType>
#include <QtDebug>
#include <cassert>

TransferFactory* TransferFactory::m_instance = 0;

Transfer* TransferFactory::createInstance(const char* clsName)
{
	if (QThread::currentThread() != thread())
	{
		Transfer* t;
		// We need to use Transfer** as Qt doesn't support return values over queued connections
		QMetaObject::invokeMethod(this, "createInstance", Qt::BlockingQueuedConnection, Q_ARG(QString, clsName), Q_ARG(Transfer**, &t));
		return t;
	}
	else
	{
		return Transfer::createInstance(clsName);
	}
}

void TransferFactory::init(Transfer* t, QString source, QString target)
{
	qDebug() << "Source: "<< source;
	qDebug() << "Target: "<< target;
	if (QThread::currentThread() != thread())
	{
		RuntimeException e;
		bool thrown;

		QMetaObject::invokeMethod(this, "init", Qt::BlockingQueuedConnection, Q_ARG(Transfer*, t), Q_ARG(QString, source), Q_ARG(QString, target), Q_ARG(RuntimeException*, &e), Q_ARG(bool*, &thrown));

		if (thrown)
			throw e;
	}
	else
	{
		return t->init(source, target);
	}
}

void TransferFactory::setState(Transfer* t, Transfer::State state)
{
	if (QThread::currentThread() != thread())
		QMetaObject::invokeMethod(this, "setStateSlot", Qt::QueuedConnection, Q_ARG(Transfer*, t), Q_ARG(Transfer::State, state));
	else
		t->setState(state);

}

void TransferFactory::setStateSlot(Transfer* t, Transfer::State state)
{
	t->setState(state);
}

void TransferFactory::createInstance(QString clsName, Transfer** t)
{
	*t = Transfer::createInstance(clsName);
}

void TransferFactory::init(Transfer* t, QString source, QString target, RuntimeException* pe, bool* eThrown)
{
	try
	{
		qDebug() << "Calling real init";
		t->init(source, target);
		*eThrown = false;
	}
	catch (const RuntimeException& e)
	{
		*pe = e;
		*eThrown = true;
	}
}

TransferFactory::TransferFactory()
{
	qRegisterMetaType<bool*>("bool*");
	qRegisterMetaType<RuntimeException*>("RuntimeException*");
	qRegisterMetaType<Transfer*>("Transfer*");
	qRegisterMetaType<Transfer**>("Transfer**");
	m_instance = this;
}

TransferFactory* TransferFactory::instance()
{
	static TransferFactory singleton;
	return &singleton;
}

Transfer* TransferFactory::createInstance(QString clsName)
{
	QByteArray ar = clsName.toLatin1();
	return createInstance(ar.data());
}
