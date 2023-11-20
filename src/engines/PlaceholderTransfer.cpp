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

#include "PlaceholderTransfer.h"

#include <QtDebug>
#include <cassert>

PlaceholderTransfer::PlaceholderTransfer(QString strClass)
    : m_strClass(strClass) {}

void PlaceholderTransfer::changeActive(bool bActive) {
  if (bActive) setState(Failed);
}

void PlaceholderTransfer::speeds(int& down, int& up) const { down = up = 0; }
qulonglong PlaceholderTransfer::total() const { return 0; }
qulonglong PlaceholderTransfer::done() const { return 0; }

QString PlaceholderTransfer::name() const {
  return tr("Error: transfer class %1 not found, this is a placeholder")
      .arg(m_strClass);
}
QString PlaceholderTransfer::myClass() const { return m_strClass; }

QString PlaceholderTransfer::message() const {
  return tr("This is a placeholder");
}

void PlaceholderTransfer::load(const QDomNode& map) {
  m_root = m_doc.importNode(map, true);
  m_doc.appendChild(m_root);

  Transfer::load(map);
}

void PlaceholderTransfer::save(QDomDocument& doc, QDomNode& map) const {
  // Transfer::save(doc, map);

  QDomNode x = doc.importNode(m_root, true);

  while (x.childNodes().count()) {
    map.appendChild(x.childNodes().item(0));
  }
}

QString PlaceholderTransfer::dataPath(bool bDirect) const { return QString(); }
