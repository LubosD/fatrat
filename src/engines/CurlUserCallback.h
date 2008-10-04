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

#ifndef CURLUSERCALLBACK_H
#define CURLUSERCALLBACK_H

#include <curl/curl.h>

class CurlUserCallback
{
public:
	virtual CURL* curlHandle() = 0;
	virtual void transferDone(CURLcode result) = 0;
	
	virtual size_t readData(char* buffer, size_t maxData) { return 0;}
	virtual bool writeData(const char* buffer, size_t bytes) { return false; }
};

#endif
