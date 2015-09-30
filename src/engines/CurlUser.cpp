/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#include "CurlUser.h"
#include "CurlPoller.h"
#include <QtDebug>

CurlUser::CurlUser()
	: m_master(0)
{
}

CurlUser::~CurlUser()
{
}

size_t CurlUser::read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This)
{
	size_t bytes = 0;
	if (ptr)
		bytes = This->readData(ptr, size*nmemb);

	This->timeProcessUp(size*nmemb);

	if(This->m_master != 0)
		This->m_master->timeProcessUp(size*nmemb);

	return bytes;
}

size_t CurlUser::write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This)
{
	bool ok = true;
	if (ptr)
		ok = This->writeData(ptr, size*nmemb);

	This->timeProcessDown(size*nmemb);

	if(This->m_master != 0)
		This->m_master->timeProcessDown(size*nmemb);

	return ok ? size*nmemb : 0;
}

void CurlUser::setSegmentMaster(CurlStat* master)
{
	m_master = master;
}

CurlStat* CurlUser::segmentMaster() const
{
	return m_master;
}

bool CurlUser::idleCycle(const timeval& tvNow)
{
	int seconds = tvNow.tv_sec - lastOperation().tv_sec;

	if(seconds > CurlPoller::getTransferTimeout())
		return false;
	else if(seconds > 1)
	{
		timeProcessDown(0);
		timeProcessUp(0);
	}
	return true;
}

size_t CurlUser::readData(char* buffer, size_t maxData)
{
	return 0;
}

bool CurlUser::writeData(const char* buffer, size_t bytes)
{
	return false;
}


