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

#ifndef PLACEHOLDERTRANSFER_H
#define PLACEHOLDERTRANSFER_H
#include <QList>

#include "Transfer.h"

class PlaceholderTransfer : public Transfer {
 public:
  PlaceholderTransfer(QString strClass);
  virtual void changeActive(bool bActive);
  virtual void speeds(int& down, int& up) const;
  virtual qulonglong total() const;
  virtual qulonglong done() const;
  virtual QString name() const;
  virtual QString myClass() const;
  virtual QString message() const;

  virtual void init(QString, QString) {}

  virtual void load(const QDomNode& map);
  virtual void save(QDomDocument& doc, QDomNode& map) const;

  void setSpeedLimits(int, int) {}
  virtual QString uri() const { return QString(); }
  virtual QString object() const { return QString(); }
  virtual void setObject(QString) {}

  virtual QString dataPath(bool bDirect = true) const;

 private:
  QString m_strClass;
  QDomNode m_root;
  QDomDocument m_doc;
};

#endif  // PLACEHOLDERTRANSFER_H
