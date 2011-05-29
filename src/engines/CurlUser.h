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
#include "CurlStat.h"
#include <curl/curl.h>
#include <QList>
#include <QPair>

class CurlUser : public CurlStat
{
public:
	CurlUser();
	virtual ~CurlUser();

	virtual size_t readData(char* buffer, size_t maxData);
	virtual bool writeData(const char* buffer, size_t bytes);
	virtual void transferDone(CURLcode result) = 0;
	virtual CURL* curlHandle() = 0;
	virtual bool idleCycle(const timeval& tvNow);

	static size_t read_function(char *ptr, size_t size, size_t nmemb, CurlUser* This);
	static size_t write_function(const char* ptr, size_t size, size_t nmemb, CurlUser* This);
protected:
	void setSegmentMaster(CurlStat* master);
	CurlStat* segmentMaster() const;

	friend class CurlPoller;
protected:
	CurlStat* m_master;
};

class CurlUserShallow : public CurlUser
{
public:
	CurlUserShallow(CURL* c) : m_curl(c) {}
	virtual void transferDone(CURLcode result) {}
	virtual CURL* curlHandle() { return m_curl; }
private:
	CURL* m_curl;
};

#endif
