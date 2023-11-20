/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef EXTENSIONMGR_H
#define EXTENSIONMGR_H
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>

#include "config.h"

#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

class ExtensionMgr : public QObject {
  Q_OBJECT
 public:
  ExtensionMgr();

  struct PackageInfo {
    QString name, desc, installedVersion, latestVersion;
  };

  QList<PackageInfo> getPackages();
 public slots:
  void loadFromServer();
 signals:
  void loaded();
  void loadFailed();
 private slots:
  void dataLoaded(QNetworkReply* req);

 private:
  QList<PackageInfo> m_packages;
  QMap<QString, QString> m_installedPackages;
  QNetworkAccessManager m_network;
};

#endif  // EXTENSIONMGR_H
