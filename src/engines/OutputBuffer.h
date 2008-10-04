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

#ifndef OUTPUTBUFFER_H
#define OUTPUTBUFFER_H
#include <QObject>

class OutputBuffer : public QObject
{
Q_OBJECT
public:
	OutputBuffer();
	~OutputBuffer();
	void putData(const char* data, unsigned long bytes);
	
	// doesn't remove anything from the buffer!
	void getData(char* data, unsigned long* bytes);
	void removeData(unsigned long bytes);
	void putBack(const char* data, unsigned long bytes);
	bool isEmpty() const { return !m_bytes; }
	unsigned long size() const { return m_bytes; }
private:
	char* m_buffer;
	unsigned long m_bytes;
};

#endif
