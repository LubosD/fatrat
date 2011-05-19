/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#include "CaptchaHttp.h"
#include "remote/HttpService.h"
#include <QTimer>

// http://svn.atomiclabs.com/pion-net/trunk/net/tests/WebServerTests.cpp

CaptchaHttp::CaptchaHttp()
{
	Captcha::registerCaptchaDecoder(this);
}

CaptchaHttp::~CaptchaHttp()
{

}

bool CaptchaHttp::process(int id, QString url)
{
	HttpService* s = HttpService::instance();
	if (!s || !s->hasCaptchaHandlers())
		return false;

	s->addCaptchaEvent(id, url);

	m_timeoutLock.lock();
	m_timeout[id] = QTime::currentTime();
	m_timeoutLock.unlock();

	QTimer::singleShot(31*1000, this, SLOT(checkTimeouts()));

	return true;
}

void CaptchaHttp::checkTimeouts()
{
	QMutexLocker l(&m_timeoutLock);
	QTime time = QTime::currentTime();

	for (QHash<int,QTime>::iterator it = m_timeout.begin(); it != m_timeout.end();)
	{
		if (it.value().secsTo(time) >= 30)
		{
			returnResult(it.key(), QString());
			it = m_timeout.erase(it);
		}
		else
			it++;
	}
}

void CaptchaHttp::captchaEntered(int id, QString text)
{
	QMutexLocker l(&m_timeoutLock);

	m_timeout.remove(id);
	this->returnResult(id, text);
}
