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

#include "JavaService.h"
#include "Queue.h"
#include <QStringList>
#include <QUuid>
#include <QCryptographicHash>
#include <arpa/inet.h>
#include <QtDebug>

const char* PASSWORD = "1234";
extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

void JavaService::processClient(QTcpSocket* client)
{
	m_bLoggedIn = false;
	
	
	qDebug() << "JavaService::processClient()";
	
	m_salt = QUuid::createUuid().toString();
	
	while(true)
	{
		QByteArray data, size;
		int len;
		
		client->waitForReadyRead(-1);
		size = client->read(4);
		
		qDebug() << "Read" << size.size() << "bytes";
		
		if(size.isEmpty())
			break;
		
		len = ntohl( *(qint32*) size.data() );
		qDebug() << len << "bytes to read";

		while(data.size() < len)
		{
			QByteArray thistime;
			thistime = client->read( len );
			data += thistime;
			
			if(thistime.isEmpty())
				client->waitForReadyRead(-1);
		}
		if(!processCommand(data, client))
			break;
	}
}

void JavaService::sendResponse(QStringList args, QTcpSocket* socket)
{
	QByteArray data;
	for(int i=0;i<args.size();i++)
	{
		args[i].replace("\n", "\\n");
		if(!data.isEmpty())
			data += '\n';
		data += args[i].toUtf8();
	}
	
	int len = htonl(data.size());
	data.prepend(QByteArray((char*)&len, 4));
	
	socket->write(data);
	socket->flush();
}

bool JavaService::processCommand(QByteArray data, QTcpSocket* socket)
{
	QStringList cmd = QString::fromUtf8(data).split('\n');
	for(int i=0;i<cmd.size();i++)
		cmd[i].replace("\\n", "\n");
	
	qDebug() << "Remote command:" << cmd[0];
	
	if(cmd[0] == "fatrat")
		sendResponse(QStringList("fatrat") << m_salt, socket);
	else if(cmd[0] == "login")
	{
		QString pass = QString(PASSWORD) + m_salt;
		QCryptographicHash hash(QCryptographicHash::Md5);
		
		qDebug() << pass;
		
		hash.addData(pass.toUtf8());
		pass = hash.result().toHex();
		
		if(cmd[1].compare(pass, Qt::CaseInsensitive))
			m_bLoggedIn = true;
		
		sendResponse(QStringList() << ((m_bLoggedIn) ? "login" : "error"), socket);
	}
	else if(cmd[0] == "queue")
	{
		if(cmd[1] == "getdata")
		{
			int qn = cmd[2].toInt();
			if(qn < 0)
				sendResponse(QStringList(cmd[0]) << QString::number(g_queues.size()), socket);
			else if(cmd.size() == 3) // qinfo
			{
				QStringList info(cmd[0]);
				QReadLocker l(&g_queuesLock);
				
				if(qn < g_queues.size() && qn >= 0)
				{
					int down, up;
					info << g_queues[qn]->name();
					g_queues[qn]->speedLimits(down, up);
					info << QString::number(down) << QString::number(up);
					g_queues[qn]->transferLimits(down, up);
					info << QString::number(down) << QString::number(up);
				}
				
				sendResponse(info, socket);
			}
		}
	}
	else if(cmd[0] == "transfer")
	{
		if(cmd[1] == "getdata" && cmd.size() == 4)
		{
			int q = cmd[2].toInt(), t = cmd[3].toInt();
			QReadLocker l(&g_queuesLock);
			
			if(q < 0 || q >= g_queues.size())
				sendResponse(QStringList("error"), socket);
			else if(t < 0)
				sendResponse(QStringList(cmd[0]) << QString::number(g_queues[q]->size()), socket);
			else
			{
				QStringList info(cmd[0]);
				Queue* qo = g_queues[q];
				Transfer* to;
				if(t >= qo->size())
				{
					sendResponse(QStringList("error"), socket);
					return true;
				}
				
				to = qo->at(t);
				
				int down, up;
				
				info << to->myClass() << to->name() << QString::number(to->primaryMode());
				info << QString::number(to->mode()) << QString::number(to->state());
				info << to->object() << QString::number(to->total()) << QString::number(to->done());
				
				to->speeds(down, up);
				info << QString::number(down) << QString::number(up);
				to->userSpeedLimits(down, up);
				info << QString::number(down) << QString::number(up);
				info << to->comment();
				
				sendResponse(info, socket);
			}
		}
	}
	
	return true;
}
