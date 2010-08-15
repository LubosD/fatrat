/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef XMLRPCSERVICE_H
#define XMLRPCSERVICE_H
#include "config.h"
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <QVariantMap>
#include "engines/OutputBuffer.h"

#ifndef WITH_WEBINTERFACE
#	error This file is not supposed to be included!
#endif

#include <pion/net/WebServer.hpp>

class XmlRpcService : public pion::net::WebService
{
public:
	void operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn);
	static void globalInit();
	static void registerFunction(QString name, QVariant (*func)(QList<QVariant>&), QVector<QVariant::Type> arguments);
	static void deregisterFunction(QString name);
protected:
	static QVariant getQueues(QList<QVariant>&);
	static QVariant Queue_getTransfers(QString uuid);
	static QVariant Queue_moveTransfers(QString uuidQueue, QStringList uuidTransfers, QString direction);
	static QVariant Transfer_setProperties(QStringList uuid, QVariantMap properties);
	static QVariant Transfer_delete(QStringList uuid, bool withData);
public:
	struct XmlRpcError
	{
		XmlRpcError(int code, QString desc)
		{
			this->code = code;
			this->desc = desc;
		}

		int code;
		QString desc;
	};
private:
	struct FunctionInfo
	{
		QVariant (*function)(QList<QVariant>&);
		QVector<QVariant::Type> arguments;
	};
	static QMap<QString,FunctionInfo> m_mapFunctions;
};

#endif
