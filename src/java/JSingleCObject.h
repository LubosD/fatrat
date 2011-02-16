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


#ifndef JSINGLECOBJECT_H
#define JSINGLECOBJECT_H

#include "config.h"
#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QReadWriteLock>
#include <QList>
#include <QtDebug>
#include <memory>
#include "JObject.h"

template <typename T> class JSingleCObject
{
public:
	JSingleCObject()
	{
		setCObject();
	}

	void setCObject()
	{
		QWriteLocker w(m_mutex.get());
		m_instances << static_cast<T*>(this);
	}

	virtual ~JSingleCObject()
	{
		QWriteLocker w(m_mutex.get());
		m_instances.removeAll(static_cast<T*>(this));
	}

	T* getCObject()
	{
		QReadLocker r(m_mutex.get());
		jobject obj = *static_cast<T*>(this);
		return getCObject(obj);
	}

	static T* getCObject(jobject jobj)
	{
		QReadLocker r(m_mutex.get());
		for (JObject* obj : m_instances)
		{
			if (obj->isSameObject(jobj))
				return static_cast<T*>(obj);
		}
		QString name = JObject(jobj).getClass().getClassName();
		return 0;
	}

private:
	JSingleCObject(const JSingleCObject<T>&) {}
	JSingleCObject<T>& operator=(const JSingleCObject<T>&) {}
private:
	static QList<JObject*> m_instances;
	static std::unique_ptr<QReadWriteLock> m_mutex;
};

#endif // JSINGLECOBJECT_H
