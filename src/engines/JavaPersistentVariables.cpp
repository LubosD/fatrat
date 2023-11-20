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

#include "JavaPersistentVariables.h"

#include "Transfer.h"

void JavaPersistentVariables::setPersistentVariable(QString key,
                                                    QVariant value) {
  m_persistent[key] = value;
}

QVariant JavaPersistentVariables::getPersistentVariable(QString key) {
  if (m_persistent.contains(key)) return m_persistent[key];
  return QVariant();
}

void JavaPersistentVariables::loadVars(const QDomNode& map) {
  QDomElement n = map.firstChildElement();
  m_persistent.clear();

  while (!n.isNull()) {
    if (n.tagName().startsWith("jpers_"))
      m_persistent[n.tagName().mid(6)] = n.text();

    n = n.nextSiblingElement();
  }
}

void JavaPersistentVariables::saveVars(QDomDocument& doc, QDomNode& map) const {
  for (QMap<QString, QVariant>::const_iterator it = m_persistent.begin();
       it != m_persistent.end(); it++)
    Transfer::setXMLProperty(doc, map, "jpers_" + it.key(),
                             it.value().toString());
}
