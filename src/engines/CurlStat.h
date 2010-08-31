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

#ifndef CURLSTAT_H
#define CURLSTAT_H
#include <QPair>
#include <sys/time.h>
#include <QReadWriteLock>

class CurlStat
{
public:
	CurlStat();
	virtual ~CurlStat();

	void speeds(int& down, int& up) const;
	void setMaxUp(int bytespersec);
	void setMaxDown(int bytespersec);

	bool hasNextReadTime() const;
	bool hasNextWriteTime() const;
	timeval nextReadTime() const;
	timeval nextWriteTime() const;

	bool performsLimiting() const;

	timeval lastOperation() const;
	void resetStatistics();

	virtual bool idleCycle(const timeval& tvNow) = 0;

	typedef QPair<long long,long> timedata_pair;

	struct SpeedData
	{
		timedata_pair* stats;
		timeval last, next, lastOp;
		timedata_pair accum;
		int max;
		int nextStat;
	};

	static const int MAX_STATS;
protected:
	static int computeSpeed(const timedata_pair* data);
	static void timeProcess(SpeedData& data, size_t bytes);
	static bool isNull(const timeval& t);

	void timeProcessDown(size_t bytes);
	void timeProcessUp(size_t bytes);
protected:
	SpeedData m_down, m_up;

	friend class CurlUser;
	friend class UrlClient;
};

#endif
