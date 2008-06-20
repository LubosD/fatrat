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

#include "SocketInterface.h"

SocketInterface::SocketInterface()
{
	memset(&m_lastProcess, 0, sizeof m_lastProcess);
}

void SocketInterface::setWantWrite()
{
	gettimeofday(&m_uploadStats.nextProcess, 0);
}

void SocketInterface::getSpeed(int& down, int& up) const
{
	down = m_downloadStats.speed;
	up = m_uploadStats.speed;
}

TransferStats::TransferStats() : transferred(0), transferredPrev(0), speed(0)
{
	memset(&nextProcess, 0, sizeof nextProcess);
}

bool TransferStats::hasNextProcess() const
{
	return nextProcess.tv_sec || nextProcess.tv_usec;
}

bool TransferStats::nextProcessDue(const timeval& now) const
{
	return (nextProcess < now) && hasNextProcess();
}

bool operator<(const timeval& t1, const timeval& t2)
{
	if(t1.tv_sec < t2.tv_sec)
		return true;
	else if(t1.tv_sec > t2.tv_sec)
		return false;
	else
		return t1.tv_usec < t2.tv_usec;
}

int operator-(const timeval& t1, const timeval& t2)
{
	return (t1.tv_sec-t2.tv_sec)*1000 + (t1.tv_usec-t2.tv_usec)/1000;
}
