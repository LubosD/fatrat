/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "LineFeeder.h"
#include <cstring>
#include <QtDebug>

static const unsigned BUFFER_SIZE = 8*1024;

LineFeeder::LineFeeder()
	: m_bytes(0)
{
	m_buffer = new char[BUFFER_SIZE];
}

LineFeeder::~LineFeeder()
{
	delete [] m_buffer;
}

void LineFeeder::feed(const char* data, unsigned long bytes)
{
	if(bytes + m_bytes > BUFFER_SIZE)
	{
		qDebug() << "LineFeeder overflow, this will have dire consequences!";
		return;
	}
	
	memcpy(m_buffer+m_bytes, data, bytes);
	m_bytes += bytes;
}

bool LineFeeder::getLine(QByteArray* line)
{
	char* p = (char*) memchr(m_buffer, '\n', m_bytes);
	if(!p)
		return false;
	
	int b = p-m_buffer-1;
	if(p > m_buffer && *(p-1) == '\r')
		b--;
	*line = QByteArray(m_buffer, b);
	
	b = p-m_buffer;
	memmove(m_buffer, p, m_bytes - b);
	m_bytes -= b;
	
	return true;
}

QByteArray LineFeeder::residue()
{
	unsigned long b = m_bytes;
	m_bytes = 0;
	return QByteArray(m_buffer, b);
}
