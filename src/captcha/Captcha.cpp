/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#include "Captcha.h"
#include <cassert>
#include <QtDebug>

QMap<int,Captcha::CaptchaProcess> Captcha::m_cb;
QList<Captcha*> Captcha::m_decoders;
int Captcha::m_next = 1;
QMutex Captcha::m_mutex (QMutex::Recursive);

int Captcha::processCaptcha(QString url, CallbackFn callback)
{
	QMutexLocker l(&m_mutex);

	qDebug() << "Captcha::processCaptcha():" << url;

	if (m_decoders.isEmpty())
	{
		callback(url, QString());
		return 0;
	}
	else
	{
		int id = m_next++;
		CaptchaProcess& prc = m_cb[id];

		prc.decodersLeft = 0;
		prc.url = url;
		prc.cb = callback;

		foreach (Captcha* c, m_decoders)
		{
			if (c->process(id, url))
				prc.decodersLeft++;
		}

		return id;
	}
}

void Captcha::abortCaptcha(int id)
{
	if (m_cb.contains(id))
	{
		// abort where available
		foreach (Captcha* c, m_decoders)
			c->abort(id);
		m_cb.remove(id);
	}
}

void Captcha::abortCaptcha(QString url)
{
	for(QMap<int,CaptchaProcess>::iterator it = m_cb.begin(); it != m_cb.end();)
	{
		if (it.value().url == url)
		{
			foreach (Captcha* c, m_decoders)
				c->abort(it.key());
			it = m_cb.erase(it);
		}
		else
			it++;
	}
}

void Captcha::registerCaptchaDecoder(Captcha* c)
{
	m_decoders << c;
}

void Captcha::returnResult(int id, QString solution)
{
	QMutexLocker l(&m_mutex);

	assert(m_cb.contains(id));

	CaptchaProcess& prc = m_cb[id];
	prc.decodersLeft--;

	if (!solution.isEmpty())
		prc.solution = solution;

	if (!prc.decodersLeft || !solution.isEmpty())
	{
		// TODO: abort others?
		if (prc.cb)
			prc.cb(prc.url, prc.solution);
		m_cb.remove(id);
	}
}
