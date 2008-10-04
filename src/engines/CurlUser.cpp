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

#include "CurlUser.h"
#include <QtDebug>

CurlUser::CurlUser()
{
}

CurlUser::~CurlUser()
{
}

size_t CurlUser::read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This)
{
	size_t bytes = This->readData(ptr, size*nmemb);
	
	This->m_statsMutex.lockForWrite();
	timeProcess(This->m_up, size*nmemb);
	This->m_statsMutex.unlock();
	
	return bytes;
}

size_t CurlUser::write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This)
{
	bool ok = This->writeData(ptr, size*nmemb);
	
	This->m_statsMutex.lockForWrite();
	timeProcess(This->m_down, size*nmemb);
	This->m_statsMutex.unlock();
	
	return ok ? size*nmemb : 0;
}

