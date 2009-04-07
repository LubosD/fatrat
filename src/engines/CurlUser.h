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
	
	timeval lastOperation() const;
protected:
	virtual CURL* curlHandle() = 0;
	virtual void transferDone(CURLcode result) = 0;
	
	virtual size_t readData(char* buffer, size_t maxData);
	virtual bool writeData(const char* buffer, size_t bytes);
	
	void resetStatistics();
	static size_t read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This);
	static size_t write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This);
	
	typedef QPair<long long,long> timedata_pair;
	
	struct SpeedData
	{
		QList<timedata_pair> stats;
		timeval last, next, lastOp;
		timedata_pair accum;
		int max;
	};
private:
	static int computeSpeed(const QList<timedata_pair>& data);
	static void timeProcess(SpeedData& data, size_t bytes);
	static bool isNull(const timeval& t);
protected:
	SpeedData m_down, m_up;
private:
	mutable QReadWriteLock m_statsMutex;
	
	friend class CurlPoller;
};

#endif
