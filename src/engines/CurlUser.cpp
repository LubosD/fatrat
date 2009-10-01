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
#include <QtDebug>

const int CurlUser::MAX_STATS = 5;

bool operator<(const timeval& t1, const timeval& t2);

CurlUser::CurlUser()
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

timeval CurlUser::lastOperation() const
{
	if(m_down.lastOp < m_up.lastOp)
		return m_up.lastOp;
	else
		return m_down.lastOp;
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
	timeval tvNow;
	gettimeofday(&tvNow, 0);
	
	if(isNull(data.last))
		data.last = tvNow;
	else
	{
		long usec = (tvNow.tv_sec-data.last.tv_sec)*1000000LL + (tvNow.tv_usec-data.last.tv_usec);
		data.accum.first += usec;
		data.accum.second += bytes;
		
		if(data.accum.first > 1000000LL)
		{
			if(data.nextStat >= MAX_STATS)
				data.nextStat = 0;
			data.stats[data.nextStat++] = data.accum;
			
			data.accum = timedata_pair(0,0);
		}
		
		memset(&data.next, 0, sizeof data.next);
		if(data.max > 0)
		{
			long delta = bytes*1000000LL/data.max - usec/2;
			
			if(delta > 0)
			{
				delta *= 2;
				//qDebug() << "Sleeping for" << delta;
				data.next = tvNow;
				data.next.tv_sec += delta/1000000LL;
				data.next.tv_usec += delta%1000000LL;
			}
		}
		
		data.last = tvNow;
	}
	
	if(bytes)
		data.lastOp = tvNow;
}

void CurlUser::resetStatistics()
{
	m_down.accum = m_up.accum = timedata_pair(0,0);
	
	memset(&m_down.last, 0, sizeof(m_down.last));
	memset(&m_up.last, 0, sizeof(m_up.last));
	
	memset(&m_down.next, 0, sizeof m_down.next);
	memset(&m_up.next, 0, sizeof m_up.next);
	
	memset(m_down.stats, 0, sizeof(*m_down.stats)*MAX_STATS);
	memset(m_up.stats, 0, sizeof(*m_up.stats)*MAX_STATS);
	m_down.nextStat = m_up.nextStat = 0;
	
	gettimeofday(&m_down.lastOp, 0);
	m_up.lastOp = m_down.lastOp;
}

int CurlUser::computeSpeed(const timedata_pair* data)
{
	long long time = 0, bytes = 0;
	
	for(int i=0;i<MAX_STATS;i++)
	{
		time += data[i].first;
		bytes += data[i].second;
	}
	
	if(time)
		return double(bytes)/(double(time)/1000000);
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
