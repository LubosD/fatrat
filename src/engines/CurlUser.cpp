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

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "CurlUser.h"
#include "CurlPoller.h"
#include <QtDebug>


bool operator<(const timeval& t1, const timeval& t2);

CurlUser::CurlUser() : m_master(0)
{
	m_down.max = m_up.max = 0;
	m_down.stats = new timedata_pair[MAX_STATS];
	m_up.stats = new timedata_pair[MAX_STATS];
	resetStatistics();
}

CurlUser::~CurlUser()
{
	delete [] m_down.stats;
	delete [] m_up.stats;
}

size_t CurlUser::readData(char* buffer, size_t maxData)
{
	return 0;
}

bool CurlUser::writeData(const char* buffer, size_t bytes)
{
	return false;
}

size_t CurlUser::read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This)
{
	size_t bytes = This->readData(ptr, size*nmemb);
	
	This->m_statsMutex.lockForWrite();
	This->timeProcessUp(size*nmemb);
	This->m_statsMutex.unlock();

	if(This->m_master != 0)
		This->m_master->timeProcessUp(size*nmemb);
	
	return bytes;
}

size_t CurlUser::write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This)
{
	bool ok;
	ok = This->writeData(ptr, size*nmemb);

	This->m_statsMutex.lockForWrite();
	This->timeProcessDown(size*nmemb);
	This->m_statsMutex.unlock();

	if(This->m_master != 0)
		This->m_master->timeProcessDown(size*nmemb);
	
	return ok ? size*nmemb : 0;
}

bool CurlUser::idleCycle(const timeval& tvNow)
{
	int seconds = tvNow.tv_sec - lastOperation().tv_sec;

	if(seconds > CurlPoller::getTransferTimeout())
		return false;
	else if(seconds > 1)
	{
		read_function(0, 0, 0, this);
		write_function(0, 0, 0, this);
	}
	return true;
}

void CurlUser::setSegmentMaster(CurlStat* master)
{
	m_master = master;
}

CurlStat* CurlUser::segmentMaster() const
{
	return m_master;
}

