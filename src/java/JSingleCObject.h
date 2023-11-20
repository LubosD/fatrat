/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef JSINGLECOBJECT_H
#define JSINGLECOBJECT_H

#include "config.h"
#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include <QHash>
#include <QReadWriteLock>
#include <QtDebug>
#include <memory>

#include "JObject.h"

extern QHash<qint64, JObject*> m_singleCObjectInstances;
extern QReadWriteLock m_singleCObjectMutex;
extern qint64 m_singleCObjectNextID;

template <typename T>
class JSingleCObject {
 public:
  JSingleCObject() { setCObject(); }

  void setCObject() {
    QWriteLocker w(&m_singleCObjectMutex);
    T* t = static_cast<T*>(this);
    m_myID = m_singleCObjectNextID++;

    m_singleCObjectInstances[m_myID] = t;
    t->setValue("nativeObjectId", JSignature::sigLong(), m_myID);
  }

  virtual ~JSingleCObject() {
    QWriteLocker w(&m_singleCObjectMutex);
    m_singleCObjectInstances.remove(m_myID);
  }

  T* getCObject() {
    QReadLocker r(&m_singleCObjectMutex);
    jobject obj = *static_cast<T*>(this);
    return getCObject(obj);
  }

  static T* getCObjectAuto(jobject jobj) {
    T* t = getCObject(jobj);
    if (t)
      return t;
    else {
      t = new T(jobj, true);
      t->setCObject();
      return t;
    }
  }

  static T* getCObject(jobject jobj) {
    QReadLocker r(&m_singleCObjectMutex);
    qint64 id = JObject(jobj)
                    .getValue("nativeObjectId", JSignature::sigLong())
                    .toLongLong();

    if (m_singleCObjectInstances.contains(id))
      return static_cast<T*>(m_singleCObjectInstances[id]);
    return 0;
  }

 private:
  JSingleCObject(const JSingleCObject<T>&) {}
  JSingleCObject<T>& operator=(const JSingleCObject<T>&) {}

  qint64 m_myID;
};

void singleCObjectRegisterNatives();

#endif  // JSINGLECOBJECT_H
