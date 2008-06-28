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

static const int MAX_STATS = 5;

CurlUser::CurlUser()
{
	m_down.max = m_up.max = 0;
	resetStatistics();
}

CurlUser::~CurlUser()
{
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

void CurlUser::timeProcess(SpeedData& data, size_t bytes)
{
	if(isNull(data.last))
		gettimeofday(&data.last, 0);
	else
	{
		timeval tvNow;
		gettimeofday(&tvNow, 0);
		
		long msec = (tvNow.tv_sec-data.last.tv_sec)*1000L + (tvNow.tv_usec-data.last.tv_usec)/1000L;
		data.accum.first += msec;
		data.accum.second += bytes;
		
		if(data.accum.first > 1000)
		{
			data.stats << data.accum;
			
			if(data.stats.size() > MAX_STATS)
				data.stats.pop_front();
			
			data.accum = QPair<long,long>(0,0);
		}
		
		memset(&data.next, 0, sizeof data.next);
		if(data.max > 0)
		{
			long delta = bytes*1000/data.max - msec/2;
			
			if(delta > 0)
			{
				delta *= 2;
				//qDebug() << "Sleeping for" << delta;
				data.next = tvNow;
				data.next.tv_sec += delta/1000;
				data.next.tv_usec += (delta%1000)*1000;
			}
		}
		
		data.last = tvNow;
	}
}

void CurlUser::resetStatistics()
{
	m_down.stats.clear();
	m_up.stats.clear();
	
	m_down.accum = m_up.accum = QPair<long,long>(0,0);
	
	memset(&m_down.last, 0, sizeof(m_down.last));
	memset(&m_up.last, 0, sizeof(m_up.last));
	
	memset(&m_down.next, 0, sizeof m_down.next);
	memset(&m_up.next, 0, sizeof m_up.next);
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
	
	down = computeSpeed(m_down.stats);
	up = computeSpeed(m_up.stats);
}

bool CurlUser::hasNextReadTime() const
{
	return !isNull(m_down.next);
}

bool CurlUser::hasNextWriteTime() const
{
	return !isNull(m_up.next);
}

timeval CurlUser::nextReadTime() const
{
	return m_down.next;
}

timeval CurlUser::nextWriteTime() const
{
	return m_up.next;
}

bool CurlUser::performsLimiting() const
{
	return m_up.max || m_down.max;
}
