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

static const int MAX_STATS = 5;

CurlUser::CurlUser()
{
	resetStatistics();
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
	
	if(isNull(This->m_lastDown))
		gettimeofday(&This->m_lastUp, 0);
	else
	{
		QWriteLocker l(&This->m_statsMutex);
		
		timeval tvNow;
		gettimeofday(&tvNow, 0);
		
		long msec = (tvNow.tv_sec-This->m_lastUp.tv_sec)*1000L + (tvNow.tv_usec-This->m_lastUp.tv_usec)/1000L;
		This->m_accumUp.first += msec;
		This->m_accumUp.second += bytes;
		
		if(This->m_accumUp.first > 1000)
		{
			This->m_statsUp << This->m_accumUp;
			
			if(This->m_statsUp.size() > MAX_STATS)
				This->m_statsUp.pop_front();
			
			This->m_accumUp = QPair<long,long>(0,0);
		}
		
		This->m_lastUp = tvNow;
	}
	
	return bytes;
}

size_t CurlUser::write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This)
{
	bool ok = This->writeData(ptr, size*nmemb);
	
	if(isNull(This->m_lastDown))
		gettimeofday(&This->m_lastDown, 0);
	else
	{
		QWriteLocker l(&This->m_statsMutex);
		
		timeval tvNow;
		gettimeofday(&tvNow, 0);
		
		long msec = (tvNow.tv_sec-This->m_lastDown.tv_sec)*1000L + (tvNow.tv_usec-This->m_lastDown.tv_usec)/1000L;
		This->m_accumDown.first += msec;
		This->m_accumDown.second += size*nmemb;
		
		if(This->m_accumDown.first > 1000)
		{
			This->m_statsDown << This->m_accumDown;
			
			if(This->m_statsDown.size() > MAX_STATS)
				This->m_statsDown.pop_front();
			
			This->m_accumDown = QPair<long,long>(0,0);
		}
		
		This->m_lastDown = tvNow;
	}
	
	return ok ? size*nmemb : 0;
}

void CurlUser::resetStatistics()
{
	m_statsDown.clear();
	m_statsUp.clear();
	
	m_accumUp = m_accumDown = QPair<long,long>(0,0);
	
	memset(&m_lastDown, 0, sizeof(m_lastDown));
	memset(&m_lastUp, 0, sizeof(m_lastUp));
}

int CurlUser::computeSpeed(const QList<QPair<long,long> >& data)
{
	long long time = 0, bytes = 0;
	
	for(int i=0;i<data.size();i++)
	{
		time += data[i].first;
		bytes += data[i].second;
	}
	
	if(time)
		return double(bytes)/time*1000;
	else
		return 0;
}

bool CurlUser::isNull(const timeval& t)
{
	return !t.tv_sec && !t.tv_usec;
}

void CurlUser::speeds(int& down, int& up) const
{
	QReadLocker l(&m_statsMutex);
	
	down = computeSpeed(m_statsDown);
	up = computeSpeed(m_statsUp);
}

bool CurlUser::shouldRead() const
{
}

bool CurlUser::shouldWrite() const
{
}
