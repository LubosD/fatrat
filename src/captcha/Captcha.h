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

#ifndef CAPTCHA_H
#define CAPTCHA_H

#include <QString>
#include <QMutex>
#include <QList>
#include <QMap>

class Captcha
{
public:
	typedef void (*CallbackFn)(QString /* url */, QString /* solution */);

	static void processCaptcha(QString url, CallbackFn fn);
	static void globalExit();
protected:
	// Captcha subclasses call its superclass
	void returnResult(int id, QString solution);
	// Captcha calls its subclasses
	virtual bool process(int id, QString url) = 0;
	static void registerCaptchaDecoder(Captcha* c);
private:
	struct CaptchaProcess
	{
		QString url, solution;
		CallbackFn cb;
		int decodersLeft;
	};

	static QMap<int,CaptchaProcess> m_cb;
	// TODO: Implement decoder priorities?
	static QList<Captcha*> m_decoders;
	static int m_next;
	static QMutex m_mutex;
};

#endif

