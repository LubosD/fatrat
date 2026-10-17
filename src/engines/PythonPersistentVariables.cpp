/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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

#include "PythonPersistentVariables.h"

#include "Transfer.h"

// Prefix of the XML elements these end up in, inside the transfer's node.
static const QLatin1String PREFIX("pyvar_");

void PythonPersistentVariables::setPersistentVariable(QString key, QVariant value)
{
	if (value.isValid())
		m_persistent[key] = value;
	else
		m_persistent.remove(key);
}

QVariant PythonPersistentVariables::getPersistentVariable(QString key) const
{
	return m_persistent.value(key);
}

void PythonPersistentVariables::loadVars(const QDomNode& map)
{
	QDomElement n = map.firstChildElement();

	m_persistent.clear();

	while (!n.isNull())
	{
		if (n.tagName().startsWith(PREFIX))
			m_persistent[n.tagName().mid(PREFIX.size())] = n.text();

		n = n.nextSiblingElement();
	}
}

void PythonPersistentVariables::saveVars(QDomDocument& doc, QDomNode& map) const
{
	for (QMap<QString, QVariant>::const_iterator it = m_persistent.begin(); it != m_persistent.end(); it++)
		Transfer::setXMLProperty(doc, map, PREFIX + it.key(), it.value().toString());
}
