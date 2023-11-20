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

#ifndef _FAKEDOWNLOAD_H
#define _FAKEDOWNLOAD_H

class FakeDownload : public Transfer {
 public:
  virtual void changeActive(bool) {}
  virtual void speeds(int& down, int& up) const {
    down = m_nDownLimitInt;
    up = m_nUpLimitInt;
  }
  virtual qulonglong total() const { return 1024 * 1024; }
  virtual qulonglong done() const { return 1024 * 512; }
  virtual QString name() const { return m_strName; }
  virtual QString myClass() const { return "FakeDownload"; }

  virtual void init(QString uri, QString) { m_strName = uri; }

  virtual void load(const QDomNode& map) {
    Transfer::load(map);
    m_strName = getXMLProperty(map, "name");
  }

  virtual void save(QDomDocument& doc, QDomNode& map) const {
    Transfer::save(doc, map);
    setXMLProperty(doc, map, "name", m_strName);
  }

  void setSpeedLimits(int, int) {}
  virtual QString uri() const { return m_strName; }
  virtual QString object() const { return QString(); }
  virtual void setObject(QString) {}

  static Transfer* createInstance() { return new FakeDownload; }
  static int acceptable(QString, bool) { return 1; }

 private:
  QString m_strName;
};

#endif
