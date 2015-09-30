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
*/

#ifndef STATICTRANSFERMESSAGE_H
#define STATICTRANSFERMESSAGE_H

template <typename T> class StaticTransferMessage : public T
{
public:
	void setMessage(QString text) { m_strMessage = text; }
	QString message() const { return m_strMessage; }
protected:
	QString m_strMessage;
};

#endif // STATICTRANSFERMESSAGE_H
