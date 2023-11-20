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

#include "CurlPollingMaster.h"

#include <QtDebug>

int CurlPollingMaster::handle() { return m_poller->handle(); }

bool CurlPollingMaster::idleCycle(const timeval& tvNow) {
  int dummy;
  QList<CurlStat*> timedOut;

  curl_multi_socket_action(m_curlm, CURL_SOCKET_TIMEOUT, 0, &dummy);

  m_usersLock.lock();
  for (sockets_hash::iterator it = m_sockets.begin(); it != m_sockets.end();
       it++) {
    CurlStat* user = it.value().second;

    if (!user->idleCycle(tvNow)) timedOut << user;
  }

  foreach (CurlStat* stat, timedOut) {
    if (CurlUser* user = dynamic_cast<CurlUser*>(stat))
      user->transferDone(CURLE_OPERATION_TIMEDOUT);
  }

  /*while(CURLMsg* msg = curl_multi_info_read(m_curlm, &dummy))
  {
          qDebug() << "CURL message:" << msg->msg;
          if(msg->msg != CURLMSG_DONE)
                  continue;

          CurlUser* user = m_users[msg->easy_handle];

          if(user)
                  user->transferDone(msg->data.result);
  }*/
  m_usersLock.unlock();

  int seconds = tvNow.tv_sec - lastOperation().tv_sec;

  if (seconds > 1) {
    timeProcessDown(0);
    timeProcessUp(0);
  }

  return true;
}
