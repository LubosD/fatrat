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

#include "XmlRpcService.h"
#include "HttpService.h"
#include "RuntimeException.h"
#include "XmlRpc.h"
#include "Queue.h"
#include "TransferHttpService.h"
#include <QReadWriteLock>
#include <QStringList>
#include <QFileInfo>
#include <pion/net/HTTPResponseWriter.hpp>
#include <QtDebug>

using namespace pion::net;

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;

QMap<QString,XmlRpcService::FunctionInfo> XmlRpcService::m_mapFunctions;

static void checkArguments(const QList<QVariant>& args, const QVariant::Type* types, int ntypes)
{
	if(args.size() != ntypes)
		throw XmlRpcService::XmlRpcError(2, QString("Invalid argument count - received %1, expected %2").arg(args.size()).arg(ntypes));
	for(int i=0;i<ntypes;i++)
	{
		if(args[i].type() != types[i])
			throw XmlRpcService::XmlRpcError(3, QString("Invalid argument type - #%1, received %2, expected %3").arg(i).arg(args[i].type()).arg(types[i]));
	}
}

static void checkType(QVariant var, QVariant::Type type)
{
	if(var.type() != type)
	{
		throw XmlRpcService::XmlRpcError(4, QString("Invalid subargument type - %1 instead of %2")
						 .arg(var.type()).arg(type));
	}
}

void XmlRpcService::globalInit()
{
	registerFunction("getQueues", XmlRpcService::getQueues, QVector<QVariant::Type>());

	{
		QVector<QVariant::Type> aa;
		aa << QVariant::String << QVariant::Map;
		registerFunction("Queue.setProperties", Queue_setProperties, aa);
	}
	{
		QVector<QVariant::Type> aa;
		aa << QVariant::Map;
		registerFunction("Queue.create", Queue_create, aa);
	}
	registerFunction("getTransferClasses", getTransferClasses, QVector<QVariant::Type>());
	{
		QVector<QVariant::Type> aa;
		aa << QVariant::String;
		registerFunction("Transfer.getAdvancedProperties", Transfer_getAdvancedProperties, aa);
	}
}

void XmlRpcService::operator()(pion::net::HTTPRequestPtr &request, pion::net::TCPConnectionPtr &tcp_conn)
{
	if (request->getMethod() != pion::net::HTTPTypes::REQUEST_METHOD_POST)
	{
		static const std::string NOT_ALLOWED_HTML_START =
				"<html><head>\n"
				"<title>405 Method Not Allowed</title>\n"
				"</head><body>\n"
				"<h1>Not Allowed</h1>\n"
				"<p>The requested method \n";
		static const std::string NOT_ALLOWED_HTML_FINISH =
				" is not allowed on this server.</p>\n"
				"</body></html>\n";
		HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *request, boost::bind(&TCPConnection::finish, tcp_conn)));
		writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
		writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_METHOD_NOT_ALLOWED);
		writer->writeNoCopy(NOT_ALLOWED_HTML_START);
		writer << request->getMethod();
		writer->writeNoCopy(NOT_ALLOWED_HTML_FINISH);
		writer->getResponse().addHeader("Allow", "GET, HEAD");
		writer->send();
		return;
	}

	QByteArray data;

	qDebug() << "XML-RPC call:" << request->getContent();

	try
	{
		QByteArray function;
		QList<QVariant> args;
		QVariant returnValue;

		XmlRpc::parseCall(request->getContent(), function, args);

		if(function == "Queue.getTransfers")
		{
			QVariant::Type aa[] = { QVariant::String };
			checkArguments(args, aa, sizeof(aa)/sizeof(aa[0]));

			returnValue = Queue_getTransfers(args[0].toString());
		}
		else if(function == "Queue.moveTransfers")
		{
			QVariant::Type aa[] = { QVariant::String, QVariant::StringList, QVariant::String };
			checkArguments(args, aa, sizeof(aa)/sizeof(aa[0]));

			returnValue = Queue_moveTransfers(args[0].toString(), args[1].toStringList(), args[2].toString());
		}
		else if(function == "Transfer.setProperties")
		{
			QVariant::Type aa[] = { QVariant::StringList, QVariant::Map };
			checkArguments(args, aa, sizeof(aa)/sizeof(aa[0]));

			returnValue = Transfer_setProperties(args[0].toStringList(), args[1].toMap());
		}
		else if(function == "Transfer.delete")
		{
			QVariant::Type aa[] = { QVariant::StringList, QVariant::Bool };
			checkArguments(args, aa, sizeof(aa)/sizeof(aa[0]));

			returnValue = Transfer_delete(args[0].toStringList(), args[1].toBool());
		}
		else if(function == "system.listMethods")
		{
			returnValue = QVariant(m_mapFunctions.keys());
		}
		else if(m_mapFunctions.contains(function))
		{
			const FunctionInfo& fi = m_mapFunctions[function];
			checkArguments(args, fi.arguments.constData(), fi.arguments.size());
			returnValue = fi.function(args);
		}
		else
			throw XmlRpcError(1, "Unknown method");

		data = XmlRpc::createResponse(returnValue);
	}
	catch(const XmlRpcError& e) // error to be reported to the client
	{
		data = XmlRpc::createFaultResponse(e.code, e.desc);
	}
	catch(const RuntimeException& e) // syntax error from XmlRpc
	{
		qDebug() << "XmlRpcService::serve():" << e.what();
		throw "400 Bad Request";
	}

	HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *request, boost::bind(&TCPConnection::finish, tcp_conn)));
	writer->write(data.data(), data.size());
	writer->send();
}

QVariant XmlRpcService::getTransferClasses(QList<QVariant>&)
{
	QVariantList rv;
	QVector<EngineEntry>* e = &g_enginesDownload;

	foreach(EngineEntry x, *e)
	{
		QVariantMap map;
		map["shortName"] = x.shortName;
		map["longName"] = x.longName;
		rv << map;
	}

	return rv;
}

QVariant XmlRpcService::Transfer_getAdvancedProperties(QList<QVariant>& args)
{
	QString uuid = args[0].toString();
	Queue* q = 0;
	Transfer* t = 0;
	QVariantMap vmap;

	HttpService::findTransfer(uuid, &q, &t);
	if (!t)
		throw XmlRpcError(102, "Invalid transfer UUID");

	TransferHttpService* s = dynamic_cast<TransferHttpService*>(t);
	if (s)
		vmap = s->properties();

	q->unlock();
	g_queuesLock.unlock();

	return vmap;
}

QVariant XmlRpcService::getQueues(QList<QVariant>&)
{
	QVariantList qlist;
	QReadLocker l(&g_queuesLock);

	for(int i=0;i<g_queues.size();i++)
	{
		Queue* q = g_queues[i];
		QVariantMap vmap;
		int up, down;

		vmap["name"] = q->name();
		vmap["uuid"] = q->uuid();
		vmap["defaultDirectory"] = q->defaultDirectory();
		vmap["moveDirectory"] = q->moveDirectory();
		vmap["upAsDown"] = q->upAsDown();

		q->transferLimits(down, up);
		vmap["transferLimits"] = QVariantList() << down << up;

		q->speedLimits(down, up);
		vmap["speedLimits"] = QVariantList() << down << up;

		qlist << vmap;
	}

	return qlist;
}

QVariant XmlRpcService::Queue_getTransfers(QString uuid)
{
	QReadLocker l(&g_queuesLock);
	Queue* q = 0;
	QVariantList vlist;

	for(int i=0;i<g_queues.size();i++)
	{
		if(g_queues[i]->uuid() == uuid)
		{
			q = g_queues[i];
			break;
		}
	}

	if(!q)
		throw XmlRpcError(101, "Invalid queue UUID");

	q->lock();

	for(int i=0;i<q->size();i++)
	{
		Transfer* t = q->at(i);
		QVariantMap vmap;
		int down, up;

		vmap["name"] = t->name();
		vmap["state"] = Transfer::state2string(t->state());
		vmap["class"] = t->myClass();
		vmap["message"] = t->message();
		vmap["mode"] = (t->mode() == Transfer::Download) ? "Download" : "Upload";
		vmap["primaryMode"] = (t->primaryMode() == Transfer::Download) ? "Download" : "Upload";
		vmap["dataPath"] = t->dataPath();
		vmap["dataPathIsDir"] = QFileInfo(t->dataPath()).isDir();
		vmap["total"] = double(t->total());
		vmap["done"] = double(t->done());
		vmap["uuid"] = t->uuid();
		vmap["comment"] = t->comment();
		vmap["object"] = t->object();
		vmap["timeRunning"] = double(t->timeRunning());

		t->speeds(down, up);
		vmap["speeds"] = QVariantList() << down << up;

		t->userSpeedLimits(down, up);
		vmap["userSpeedLimits"] = QVariantList() << down << up;

		TransferHttpService* srv = dynamic_cast<TransferHttpService*>(t);
		if (srv)
			vmap["detailsScript"] = srv->detailsScript();

		vlist << vmap;
	}

	q->unlock();
	return vlist;
}

QVariant XmlRpcService::Queue_create(QList<QVariant>& args)
{
	QVariantMap properties = args[0].toMap();
	Queue* q = new Queue;

	for(QVariantMap::const_iterator it = properties.constBegin(); it != properties.constEnd(); it++)
	{
		QString prop = it.key();
		if(prop == "name")
		{
			checkType(it.value(), QVariant::String);
			q->setName(it.value().toString());
		}
		else if(prop == "defaultDirectory")
		{
			checkType(it.value(), QVariant::String);
			q->setDefaultDirectory(it.value().toString());
		}
		else if(prop == "moveDirectory")
		{
			checkType(it.value(), QVariant::String);
			q->setMoveDirectory(it.value().toString());
		}
		else if(prop == "speedLimits")
		{
			checkType(it.value(), QVariant::List);
			QVariantList list = it.value().toList();

			if(list.size() != 2)
				throw XmlRpcError(104, QString("Invalid list length: %1").arg(list.size()));

			checkType(list.at(0), QVariant::Int);
			checkType(list.at(1), QVariant::Int);

			q->setSpeedLimits(list.at(0).toInt(), list.at(1).toInt());
		}
		else if(prop == "transferLimits")
		{
			checkType(it.value(), QVariant::List);
			QVariantList list = it.value().toList();

			if(list.size() != 2)
				throw XmlRpcError(104, QString("Invalid list length: %1").arg(list.size()));

			checkType(list.at(0), QVariant::Int);
			checkType(list.at(1), QVariant::Int);

			q->setTransferLimits(list.at(0).toInt(), list.at(1).toInt());
		}
		else if(prop == "upAsDown")
		{
			checkType(it.value(), QVariant::Bool);
			q->setUpAsDown(it.value().toBool());
		}
		else
			throw XmlRpcError(103, QString("Invalid transfer property: %1").arg(prop));
	}

	QWriteLocker l(&g_queuesLock);
	g_queues << q;

	return QVariant();
}

QVariant XmlRpcService::Queue_setProperties(QList<QVariant>& args)
{
	QString uuid = args[0].toString();
	QVariantMap properties = args[1].toMap();
	Queue* q = 0;
	QReadLocker r(&g_queuesLock);

	HttpService::findQueue(uuid, &q);

	if(!q)
		throw XmlRpcError(101, "Invalid queue UUID");

	for(QVariantMap::const_iterator it = properties.constBegin(); it != properties.constEnd(); it++)
	{
		QString prop = it.key();
		if(prop == "name")
		{
			checkType(it.value(), QVariant::String);
			q->setName(it.value().toString());
		}
		else if(prop == "defaultDirectory")
		{
			checkType(it.value(), QVariant::String);
			q->setDefaultDirectory(it.value().toString());
		}
		else if(prop == "moveDirectory")
		{
			checkType(it.value(), QVariant::String);
			q->setMoveDirectory(it.value().toString());
		}
		else if(prop == "speedLimits")
		{
			checkType(it.value(), QVariant::List);
			QVariantList list = it.value().toList();

			if(list.size() != 2)
				throw XmlRpcError(104, QString("Invalid list length: %1").arg(list.size()));

			checkType(list.at(0), QVariant::Int);
			checkType(list.at(1), QVariant::Int);

			q->setSpeedLimits(list.at(0).toInt(), list.at(1).toInt());
		}
		else if(prop == "transferLimits")
		{
			checkType(it.value(), QVariant::List);
			QVariantList list = it.value().toList();

			if(list.size() != 2)
				throw XmlRpcError(104, QString("Invalid list length: %1").arg(list.size()));

			checkType(list.at(0), QVariant::Int);
			checkType(list.at(1), QVariant::Int);

			q->setTransferLimits(list.at(0).toInt(), list.at(1).toInt());
		}
		else if(prop == "upAsDown")
		{
			checkType(it.value(), QVariant::Bool);
			q->setUpAsDown(it.value().toBool());
		}
		else
			throw XmlRpcError(103, QString("Invalid transfer property: %1").arg(prop));
	}

	return QVariant();
}

QVariant XmlRpcService::Transfer_setProperties(QStringList luuid, QVariantMap properties)
{
	Queue* q = 0;
	Transfer* t = 0;

	foreach (QString uuid, luuid)
	{
		HttpService::findTransfer(uuid, &q, &t);

		if(!t)
			throw XmlRpcError(102, "Invalid transfer UUID");

		for(QVariantMap::const_iterator it = properties.constBegin(); it != properties.constEnd(); it++)
		{
			QString prop = it.key();
			if(prop == "state")
			{
				checkType(it.value(), QVariant::String);
				t->setState(Transfer::string2state(it.value().toString()));
			}
			else if(prop == "comment")
			{
				checkType(it.value(), QVariant::String);
				t->setComment(it.value().toString());
			}
			else if(prop == "object")
			{
				checkType(it.value(), QVariant::String);
				t->setObject(it.value().toString());
			}
			else if(prop == "userSpeedLimits")
			{
				checkType(it.value(), QVariant::List);
				QVariantList list = it.value().toList();

				if(list.size() != 2)
					throw XmlRpcError(104, QString("Invalid list length: %1").arg(list.size()));

				checkType(list.at(0), QVariant::Int);
				checkType(list.at(1), QVariant::Int);

				t->setUserSpeedLimits(list.at(0).toInt(), list.at(1).toInt());
			}
			else
				throw XmlRpcError(103, QString("Invalid transfer property: %1").arg(prop));
		}

		q->unlock();
		g_queuesLock.unlock();
	}
	return QVariant();
}

QVariant XmlRpcService::Transfer_delete(QStringList luuid, bool withData)
{
	Queue* q = 0;
	Transfer* t = 0;

	foreach (QString uuid, luuid)
	{
		int pos = HttpService::findTransfer(uuid, &q, &t);
		qDebug() << "Found transfer at" << pos;

		if(!t)
			throw XmlRpcError(102, "Invalid transfer UUID");

		if (withData)
			q->removeWithData(pos, true);
		else
			q->remove(pos, true);

		q->unlock();
		g_queuesLock.unlock();
	}
}

QVariant XmlRpcService::Queue_moveTransfers(QString uuidQueue, QStringList uuidTransfers, QString direction)
{
	Queue* q;
	HttpService::findQueue(uuidQueue, &q);

	if (!q)
		throw XmlRpcError(101, "Invalid queue UUID");

	QList<int> positions;
	q->lockW();
	for(int i=0;i<q->size();i++)
	{
		for(int j=0;j<uuidTransfers.size();j++)
		{
			if (uuidTransfers[j] == q->at(i)->uuid())
			{
				positions << i;
				uuidTransfers.removeAt(j);
				break;
			}
		}
	}

	if (!uuidTransfers.empty())
	{
		q->unlock();
		g_queuesLock.unlock();
		throw XmlRpcError(102, "One or more invalid transfer UUIDs");
	}

	qSort(positions.begin(), positions.end());

	direction = direction.toLower();
	if (direction == "up")
	{
		foreach(int pos, positions)
			q->moveUp(pos, true);
	}
	else if (direction == "bottom")
	{
		int x = 0;
		foreach(int pos, positions)
		{
			q->moveToBottom(pos-x, true);
			x++;
		}
	}
	else if (direction == "top")
	{
		for(int i=positions.size()-1,x=0;i >= 0;i--,x++)
			q->moveToTop(positions[i]+x, true);
	}
	else if (direction == "down")
	{
		for(int i=positions.size()-1;i >= 0;i--)
			q->moveDown(positions[i], true);
	}
	else
	{
		q->unlock();
		g_queuesLock.unlock();
		throw XmlRpcError(105, "Invalid move direction");
	}

	q->unlock();
	g_queuesLock.unlock();

	return QVariant();
}

void XmlRpcService::registerFunction(QString name, QVariant (*func)(QList<QVariant>&), QVector<QVariant::Type> arguments)
{
	FunctionInfo fi;
	fi.arguments = arguments;
	fi.function = func;

	m_mapFunctions[name] = fi;
}

void XmlRpcService::deregisterFunction(QString name)
{
	m_mapFunctions.remove(name);
}
