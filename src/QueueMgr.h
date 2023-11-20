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

#ifndef _QUEUEMGR_H
#define _QUEUEMGR_H
#include <QMap>
#include <QSettings>
#include <QThread>
#include <QTimer>

#include "Queue.h"

class QueueMgr : public QObject {
  Q_OBJECT
 public:
  QueueMgr();
  void exit();

  static QueueMgr* instance() { return m_instance; }

  inline int totalDown() const { return m_down; }
  inline int totalUp() const { return m_up; }

  void pauseAllTransfers();
  void unpauseAllTransfers();
  inline bool isAllPaused() { return !m_paused.isEmpty(); }

 private:
  void doMove(Queue* q, Transfer* t);
  static Queue* findQueue(Transfer* t);
 public slots:
  void doWork();
  void transferStateChanged(Transfer*, Transfer::State, Transfer::State);
  void transferModeChanged(Transfer*, Transfer::Mode, Transfer::Mode);

 private:
  static QueueMgr* m_instance;
  QTimer* m_timer;
  int m_nCycle;
  int m_down, m_up;

  // for the Pause all feature
  QMap<QUuid, Transfer::State> m_paused;
};

#endif
