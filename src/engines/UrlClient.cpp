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

#include "UrlClient.h"

UrlClient::UrlClient()
	: m_progress(0), m_target(0), m_rangeFrom(0), m_rangeTo(-1), m_curl(0)
{
}

UrlClient::~UrlClient()
{
}

void UrlClient::setSourceObject(const UrlObject& obj)
{
	m_source = obj;
}

void UrlClient::setTargetObject(QIODevice* dev)
{
	m_target = dev;
}

void UrlClient::setRange(qlonglong from, qlonglong to)
{
	m_rangeFrom = from;
	m_rangeTo = to;
}

qlonglong UrlClient::progress() const
{
	return m_progress;
}
