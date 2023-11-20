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

#ifndef DBUSIMPL_H
#define DBUSIMPL_H
#include <QObject>
#include <QStringList>

class DbusImpl : public QObject {
  Q_OBJECT
 public:
  DbusImpl();
  static DbusImpl* instance() { return m_instance; }
 public slots:
  void addTransfers(QString uris);
  QString addTransfersNonInteractive(QString uris, QString target,
                                     QString className, int queueID);

  // workaround for QHttp Qt bug - receiving side
  void addTransfersNonInteractive2(QString uris, QString target,
                                   QString className, int queueID,
                                   QString* resp);
  QStringList getQueues();

 public:
  // workaround for QHttp Qt bug - emiting side
  QString addTransfers(QString uris, QString target, QString className,
                       int queueID);

 private:
  static DbusImpl* m_instance;
};

#endif
