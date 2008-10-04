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

#include "OutputBuffer.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

OutputBuffer::OutputBuffer(unsigned long reserve)
	: m_buffer(0), m_bytes(0), m_reserve(reserve)
{
	m_buffer = (char*) malloc(reserve);
}

OutputBuffer::~OutputBuffer()
{
	free(m_buffer);
}

void OutputBuffer::putData(const char* data, unsigned long bytes)
{
	if(m_bytes+bytes > m_reserve)
		m_buffer = (char*) realloc(m_buffer, m_bytes+bytes);
	memcpy(m_buffer+m_bytes, data, bytes);
	m_bytes += bytes;
}

void OutputBuffer::putBack(const char* data, unsigned long bytes)
{
	if(m_bytes+bytes > m_reserve)
		m_buffer = (char*) realloc(m_buffer, m_bytes+bytes);
	memmove(m_buffer+bytes, m_buffer, m_bytes);
	memcpy(m_buffer, data, bytes);
	m_bytes += bytes;
}

void OutputBuffer::getData(char* data, unsigned long* bytes) const
{
	*bytes = std::min<unsigned long>(*bytes, m_bytes);
	memcpy(data, m_buffer, *bytes);
}

void OutputBuffer::removeData(unsigned long bytes)
{
	bytes = std::min<unsigned long>(bytes, m_bytes);
	memmove(m_buffer, m_buffer+bytes, m_bytes-bytes);
	m_bytes -= bytes;
	//m_buffer = (char*) realloc(m_buffer, m_bytes);
}

