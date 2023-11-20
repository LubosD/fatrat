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

#include "CurlPoller.h"

#include <QtDebug>
#include <cassert>

#include "CurlPollingMaster.h"

CurlPoller* CurlPoller::m_instance = 0;

int CurlPoller::m_nTransferTimeout = 20;

CurlPoller::CurlPoller()
    : m_bAbort(false), m_timeout(0), m_usersLock(QMutex::Recursive) {
  m_curlTimeout = 0;
  curl_global_init(CURL_GLOBAL_SSL);
  m_curlm = curl_multi_init();
  m_poller = Poller::createInstance(this);

  curl_multi_setopt(m_curlm, CURLMOPT_SOCKETFUNCTION, socket_callback);
  curl_multi_setopt(m_curlm, CURLMOPT_SOCKETDATA,
                    static_cast<CurlPoller*>(this));

  if (!m_instance) {
    m_instance = this;
    start();
  }
}

CurlPoller::~CurlPoller() {
  m_bAbort = true;

  if (isRunning()) wait();

  if (this == m_instance) m_instance = 0;
  curl_multi_cleanup(m_curlm);
  curl_global_cleanup();
}

bool operator<(const timeval& t1, const timeval& t2) {
  if (t1.tv_sec < t2.tv_sec)
    return true;
  else if (t1.tv_sec > t2.tv_sec)
    return false;
  else
    return t1.tv_usec < t2.tv_usec;
}

void CurlPoller::pollingCycle(bool oneshot) {
  Poller::Event events[30];
  int dummy;
  timeval tvNow;
  QList<CurlStat*> timedOut;

  int numEvents = m_poller->wait(!oneshot ? m_timeout : 0, events,
                                 sizeof(events) / sizeof(events[0]));

  m_usersLock.lock();
  if (!numEvents) {
    curl_multi_socket_action(m_curlm, CURL_SOCKET_TIMEOUT, 0, &dummy);
  }

  for (int i = 0; i < numEvents; i++) {
    int socket = events[i].socket;
    if (!m_masters.contains(socket)) {
      int mask = 0;

      if (events[i].flags & Poller::PollerIn) mask |= CURL_CSELECT_IN;
      if (events[i].flags & Poller::PollerOut) mask |= CURL_CSELECT_OUT;
      if (events[i].flags & (Poller::PollerError | Poller::PollerHup))
        mask |= CURL_CSELECT_ERR;

      curl_multi_socket_action(m_curlm, socket, mask, &dummy);
    } else
      m_masters[socket]->pollingCycle(true);
  }

  gettimeofday(&tvNow, 0);

  if (m_curlTimeout <= 0 || m_curlTimeout > 500)
    m_timeout = 500;
  else
    m_timeout = m_curlTimeout;

  while (!m_queueToDelete.isEmpty()) {
    CurlUser* c = m_queueToDelete.dequeue();
    CURL* handle = c->curlHandle();

    qDebug() << "Deleting a queued CURL object:" << c << handle;
    assert(!m_users.contains(handle));

    for (sockets_hash::iterator it = m_sockets.begin(); it != m_sockets.end();
         it++) {
      if (it.value().second == c) m_socketsToRemove << it.key();
    }

    curl_multi_remove_handle(m_curlm, handle);
    curl_easy_cleanup(handle);
    delete c;
    assert(!m_queueToDelete.contains(c));
  }

  for (int i = 0; i < m_socketsToRemove.size(); i++)
    m_sockets.remove(m_socketsToRemove[i]);
  m_socketsToRemove.clear();

  for (sockets_hash::iterator it = m_socketsToAdd.begin();
       it != m_socketsToAdd.end(); it++)
    m_sockets[it.key()] = it.value();
  m_socketsToAdd.clear();

  for (sockets_hash::iterator it = m_sockets.begin(); it != m_sockets.end();
       it++) {
    int mask = 0;
    int msec = -1;
    CurlStat* user = it.value().second;

    if (!user->idleCycle(tvNow)) timedOut << user;

    if (user->hasNextReadTime()) {
      if (user->nextReadTime() < tvNow) mask |= CURL_CSELECT_IN;
      timeval tv = user->nextReadTime();
      msec = (tv.tv_sec - tvNow.tv_sec) * 1000 +
             (tv.tv_usec - tvNow.tv_usec) / 1000;
    }
    if (user->hasNextWriteTime()) {
      if (user->nextWriteTime() < tvNow) mask |= CURL_CSELECT_OUT;
      int mmsec;
      timeval tv = user->nextWriteTime();
      mmsec = (tv.tv_sec - tvNow.tv_sec) * 1000 +
              (tv.tv_usec - tvNow.tv_usec) / 1000;

      if (mmsec < msec || msec < 0) msec = mmsec;
    }

    if (mask) curl_multi_socket_action(m_curlm, it.key(), mask, &dummy);

    int& flags = it.value().first;
    if (msec > 0) {
      if (msec < m_timeout) m_timeout = msec;
      if (!(flags & Poller::PollerOneShot)) {
        m_poller->removeSocket(it.key());
        flags |= Poller::PollerOneShot;
      }
    } else {
      if (user->performsLimiting()) {
        flags |= Poller::PollerOneShot;
      } else if (flags & Poller::PollerOneShot)
        flags ^= Poller::PollerOneShot;
      else
        continue;
      m_poller->addSocket(it.key(), flags);
    }
  }

  while (CURLMsg* msg = curl_multi_info_read(m_curlm, &dummy)) {
    qDebug() << "CURL message:" << msg->msg;
    if (msg->msg != CURLMSG_DONE) continue;

    CurlUser* user = m_users[msg->easy_handle];

    if (user) user->transferDone(msg->data.result);
  }

  foreach (CurlStat* stat, timedOut) {
    if (CurlUser* user = dynamic_cast<CurlUser*>(stat))
      user->transferDone(CURLE_OPERATION_TIMEDOUT);
  }

  for (QMap<int, CurlPollingMaster*>::const_iterator it = m_masters.begin();
       it != m_masters.end(); it++) {
    CurlPoller* p = it.value();
    p->checkErrors(tvNow);
  }

  m_usersLock.unlock();
}

void CurlPoller::checkErrors(timeval tvNow) {
  QMutexLocker l(&m_usersLock);
  QList<CurlStat*> timedOut;
  int dummy;

  for (sockets_hash::iterator it = m_sockets.begin(); it != m_sockets.end();
       it++) {
    CurlStat* user = it.value().second;

    if (!user->idleCycle(tvNow)) timedOut << user;
  }

  while (CURLMsg* msg = curl_multi_info_read(m_curlm, &dummy)) {
    qDebug() << "CURL message:" << msg->msg;
    if (msg->msg != CURLMSG_DONE) continue;

    CurlUser* user = m_users[msg->easy_handle];

    if (user) user->transferDone(msg->data.result);
  }

  foreach (CurlStat* stat, timedOut) {
    if (CurlUser* user = dynamic_cast<CurlUser*>(stat))
      user->transferDone(CURLE_OPERATION_TIMEDOUT);
  }
}

void CurlPoller::run() {
  curl_multi_setopt(m_curlm, CURLMOPT_TIMERFUNCTION, timer_callback);
  curl_multi_setopt(m_curlm, CURLMOPT_TIMERDATA, &m_curlTimeout);

  while (!m_bAbort) pollingCycle(false);
}

void CurlPoller::epollEnable(int socket, int events) {
  m_poller->addSocket(socket, events);
}

int CurlPoller::timer_callback(CURLM* multi, long newtimeout, long* timeout) {
  *timeout = newtimeout;
  return 0;
}

int CurlPoller::socket_callback(CURL* easy, curl_socket_t s, int action,
                                CurlPoller* This, void* socketp) {
  int flags = Poller::PollerOneShot | Poller::PollerError | Poller::PollerHup;

  if (action == CURL_POLL_IN || action == CURL_POLL_INOUT)
    flags |= Poller::PollerIn;
  if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT)
    flags |= Poller::PollerOut;

  if (action == CURL_POLL_REMOVE) {
    qDebug() << "CurlPoller::socket_callback - remove";

    This->m_socketsToRemove << s;
    return This->m_poller->removeSocket(s);
  } else {
    qDebug() << "CurlPoller::socket_callback - add/mod" << s << flags;

    This->m_socketsToAdd[s] = QPair<int, CurlStat*>(
        flags, static_cast<CurlStat*>(This->m_users[easy]));

    return This->m_poller->addSocket(s, flags);
  }
}

void CurlPoller::addTransfer(CurlUser* obj) {
  QMutexLocker locker(&m_usersLock);

  CURL* handle = obj->curlHandle();
  qDebug() << "CurlPoller::addTransfer" << obj << handle;

  obj->resetStatistics();
  m_users[handle] = obj;
  curl_multi_add_handle(m_curlm, handle);
}

void CurlPoller::removeTransfer(CurlUser* obj, bool nodeep) {
  QMutexLocker locker(&m_usersLock);

  qDebug() << "CurlPoller::removeTransfer" << obj << obj->curlHandle();

  CURL* handle = obj->curlHandle();
  if (handle != 0) {
    if (!nodeep) {
      assert(!m_queueToDelete.contains(obj));
      m_queueToDelete.enqueue(obj);
      m_users.remove(handle);
      assert(!m_users.contains(handle));
    } else {
      CurlUserShallow* s = new CurlUserShallow(handle);
      m_queueToDelete.enqueue(s);
      m_users.remove(handle);
    }
  }
}

/*void CurlPoller::removeSafely(CURL* curl)
{
        QMutexLocker locker(&m_usersLock);
        m_queueToDelete.enqueue(curl);
}*/

void CurlPoller::setTransferTimeout(int timeout) {
  m_nTransferTimeout = timeout;
}

void CurlPoller::addTransfer(CurlPollingMaster* obj) {
  QMutexLocker locker(&m_usersLock);

  int handle = obj->handle();
  int mask = Poller::PollerError | Poller::PollerHup | Poller::PollerIn |
             Poller::PollerOut;

  qDebug() << "Adding a polling master" << handle << obj;
  m_masters[handle] = obj;
  m_sockets[handle] = QPair<int, CurlStat*>(mask, obj);
  m_poller->addSocket(handle, mask);
}

void CurlPoller::removeTransfer(CurlPollingMaster* obj) {
  QMutexLocker locker(&m_usersLock);

  int handle = obj->handle();
  m_masters.remove(handle);
  m_sockets.remove(handle);
  m_poller->removeSocket(handle);
}
