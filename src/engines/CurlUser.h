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


#ifndef CURLUSER_H
#define CURLUSER_H
#include <curl/curl.h>
#include <sys/time.h>
#include <QReadWriteLock>
#include <QList>
#include <QPair>

class CurlUser
{
public:
	CurlUser();
	virtual ~CurlUser();
	
	void speeds(int& down, int& up) const;
	
	bool hasNextReadTime() const;
	bool hasNextWriteTime() const;
	timeval nextReadTime() const;
	timeval nextWriteTime() const;
	
	bool performsLimiting() const;
protected:
	virtual CURL* curlHandle() = 0;
	virtual void transferDone(CURLcode result) = 0;
	
	virtual size_t readData(char* buffer, size_t maxData);
	virtual bool writeData(const char* buffer, size_t bytes);
	
	void resetStatistics();
	static size_t read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This);
	static size_t write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This);
	
	struct SpeedData
	{
		QList<QPair<long,long> > stats;
		timeval last, next;
		QPair<long, long> accum;
		int max;
	};
	
private:
	static int computeSpeed(const QList<QPair<long,long> >& data);
	static void timeProcess(SpeedData& data, size_t bytes);
	static bool isNull(const timeval& t);
protected:
	SpeedData m_down, m_up;
private:
	mutable QReadWriteLock m_statsMutex;
	
	friend class CurlPoller;
};

#endif
