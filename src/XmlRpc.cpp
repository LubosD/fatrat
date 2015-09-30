/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2009 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "XmlRpc.h"
#include "RuntimeException.h"
#include <QDomDocument>
#include <QDateTime>
#include <QStringList>
#include <QtDebug>

static void insertArgument(QDomDocument& doc, QDomElement& where, const QVariant& what);
static void insertValue(QDomDocument& doc, QDomElement& where, QString type, QString value);
static QVariant parseValue(const QDomElement& where);
static QByteArray createCallOrResponse(const QList<QVariant>& args, QByteArray* function = 0);

QByteArray XmlRpc::createCall(QByteArray function, const QList<QVariant>& args)
{
	return createCallOrResponse(args, &function);
}

QByteArray createCallOrResponse(const QList<QVariant>& args, QByteArray* function)
{
	QDomDocument doc;
	QDomElement root, sub;
	QDomText text;

	root = doc.createElement(function ? "methodCall" : "methodResponse");
	doc.appendChild(root);

	if(function != 0)
	{
		sub = doc.createElement("methodName");
		text = doc.createTextNode(*function);
		sub.appendChild(text);
		root.appendChild(sub);
	}

	sub = doc.createElement("params");
	foreach(QVariant v, args)
	{
		if (v.isNull())
			continue;

		QDomElement param = doc.createElement("param");
		insertArgument(doc, param, v);
		sub.appendChild(param);
	}
	root.appendChild(sub);

	return doc.toByteArray(-1);
}

void insertArgument(QDomDocument& doc, QDomElement& where, const QVariant& what)
{
	switch(what.type())
	{
	case QMetaType::Bool:
		insertValue(doc, where, "boolean", what.toBool() ? "1" : "0");
		break;
	case QMetaType::Int:
	case QMetaType::UInt:
//	case QMetaType::Long:
//	case QMetaType::Short:
//	case QMetaType::ULong:
//	case QMetaType::UShort:
		insertValue(doc, where, "i4", what.toString());
		break;
	case QMetaType::ULongLong:
	case QMetaType::LongLong:
		insertValue(doc, where, "ex:i8", what.toString());
		break;
//	case QMetaType::Float:
	case QMetaType::Double:
		insertValue(doc, where, "double", what.toString());
		break;
	case QMetaType::QString:
		insertValue(doc, where, "string", what.toString());
		break;
	case QMetaType::QByteArray:
		insertValue(doc, where, "base64", what.toByteArray().toBase64());
		break;
	case QMetaType::QDateTime:
		insertValue(doc, where, "dateTime.iso8601", what.toDateTime().toString(Qt::ISODate));
		break;
	case QMetaType::QVariantList:
		{
			QDomElement array, data, value;
			value = doc.createElement("value");
			array = doc.createElement("array");
			data = doc.createElement("data");
			
			QList<QVariant> list = what.toList();
			foreach(QVariant v, list)
				insertArgument(doc, data, v);
			
			array.appendChild(data);
			value.appendChild(array);
			where.appendChild(value);
		}
		break;
	case QMetaType::QStringList:
		{
			QDomElement array, data, value;
			value = doc.createElement("value");
			array = doc.createElement("array");
			data = doc.createElement("data");

			QStringList list = what.toStringList();
			foreach(QString v, list)
				insertArgument(doc, data, v);

			array.appendChild(data);
			value.appendChild(array);
			where.appendChild(value);
		}
		break;
	case QMetaType::QVariantMap:
		{
			QDomElement _struct, value;
			_struct = doc.createElement("struct");
			value = doc.createElement("value");
			
			QMap<QString, QVariant> map = what.toMap();
			for(QMap<QString, QVariant>::iterator it = map.begin(); it != map.end(); it++)
			{
				QDomElement member = doc.createElement("member");
				QDomElement name = doc.createElement("name");
				QDomText ntext = doc.createTextNode(it.key());
				
				name.appendChild(ntext);
				member.appendChild(name);
				insertArgument(doc, member, it.value());
				_struct.appendChild(member);
			}
			
			value.appendChild(_struct);
			where.appendChild(value);
		}
		break;
	default:
		qDebug() << "XmlRpc::insertArgument(): Unsupported QVariant type:" << what.type();
	}
}

void insertValue(QDomDocument& doc, QDomElement& where, QString type, QString value)
{
	QDomElement child = doc.createElement("value");
	QDomElement v;

	if (type.startsWith("ex:"))
		v = doc.createElementNS("http://ws.apache.org/xmlrpc/namespaces/extensions", type);
	else
		v = doc.createElement(type);

	QDomText text = doc.createTextNode(value);
	v.appendChild(text);
	child.appendChild(v);
	where.appendChild(child);
}

QVariant XmlRpc::parseResponse(const QByteArray& response)
{
	QDomDocument doc;
	QDomElement root;
	QString msg;
	bool bFault;
	
	if(!doc.setContent(response, false, &msg))
		throw RuntimeException(msg);
	
	root = doc.documentElement();
	if(root.tagName() != "methodResponse")
		throw RuntimeException(QObject::tr("Not a XML-RPC response"));
	
	QDomElement e = root.firstChildElement();
	if(e.tagName() == "fault")
		bFault = true;
	else if(e.tagName() == "params")
		bFault = false;
	else
		throw RuntimeException(QObject::tr("Not a XML-RPC response"));
	
	if(!bFault)
	{
		QDomElement p = e.firstChildElement("param");
		if(p.isNull())
			return QVariant();

		QDomElement value = p.firstChildElement("value");
		if(value.isNull())
			throw RuntimeException(QObject::tr("Invalid XML-RPC response"));
			
		return parseValue(value);
	}
	else
	{
		QDomElement value = e.firstChildElement("value");
		if(value.isNull())
			throw RuntimeException(QObject::tr("Server is indicating an unknown failure"));
		
		QMap<QString, QVariant> v = parseValue(value).toMap();
		throw RuntimeException(QString("%1 %2").arg(v["faultCode"].toString()).arg(v["faultString"].toString()));
	}
}

QVariant parseValue(const QDomElement& where)
{
	QDomElement ce = where.firstChildElement();
	QString c = ce.tagName();
	QString contents;
	
	if(c.isNull())
	{
		contents = where.text();
		c = "string";
	}
	else
		contents = ce.firstChild().toText().data();
	
	if(c == "boolean")
		return contents.toInt() != 0;
	else if(c == "i4" || c == "int")
		return contents.toInt();
	else if(c == "double")
		return contents.toDouble();
	else if(c == "string")
		return contents;
	else if(c == "base64")
		return QByteArray::fromBase64(contents.toLatin1());
	else if(c == "dateTime.iso8601")
		return QDateTime::fromString(contents, Qt::ISODate);
	else if(c == "array")
	{
		QDomElement data = ce.firstChildElement("data");
		if(data.isNull())
			return QVariant();
		
		QDomElement v = data.firstChildElement("value");
		QList<QVariant> variantList;
		bool allstring = true;
		
		while(!v.isNull())
		{
			QVariant parsed = parseValue(v);
			variantList << parsed;

			allstring &= parsed.type() == QVariant::String;

			v = v.nextSiblingElement("value");
		}

		if (allstring && !variantList.empty())
		{
			QVariant var(variantList);
			var.convert(QVariant::StringList);
			return var;
		}
		
		return variantList;
	}
	else if(c == "struct")
	{
		QDomElement v = ce.firstChildElement("member");
		QMap<QString, QVariant> variantMap;
		
		while(!v.isNull())
		{
			QDomElement name, value;
			name = v.firstChildElement("name");
			value = v.firstChildElement("value");
			
			if(!name.isNull() && !value.isNull())
			{
				QString n = name.firstChild().toText().data();
				variantMap[n] = parseValue(value);
			}
			else
				qDebug() << "XmlRpc::parseValue(): invalid struct member encountered";
			
			v = v.nextSiblingElement("member");
		}
		
		return variantMap;
	}
	else
	{
		qDebug() << "XmlRpc::parseValue(): Unknown data type:" << c;
		return QVariant();
	}
}

QByteArray XmlRpc::createResponse(const QVariant& value)
{
	return createCallOrResponse(QList<QVariant>() << value);
}

QByteArray XmlRpc::createFaultResponse(int code, const QString& error)
{
	QDomDocument doc;
	QDomElement root, sub;
	QDomText text;

	root = doc.createElement("methodResponse");
	doc.appendChild(root);

	sub = doc.createElement("fault");

	QVariantMap vmap;
	vmap["faultCode"] = code;
	vmap["faultString"] = error;

	insertArgument(doc, sub, vmap);
	root.appendChild(sub);

	return doc.toByteArray(-1);
}

void XmlRpc::parseCall(const QByteArray& call, QByteArray& function, QList<QVariant>& args)
{
	QDomDocument doc;
	QDomElement root;
	QString msg;
	QList<QVariant> values;

	if(!doc.setContent(call, false, &msg))
		throw RuntimeException(msg);

	root = doc.documentElement();
	if(root.tagName() != "methodCall")
		throw RuntimeException(QObject::tr("Not a XML-RPC call"));

	QDomElement methodName = root.firstChildElement("methodName");
	if(methodName.isNull())
		throw RuntimeException(QObject::tr("Not a valid XML-RPC call"));
	function = methodName.firstChild().toText().data().toUtf8();

	QDomElement e = root.firstChildElement("params");
	if(!e.isNull())
	{
		QDomElement p = e.firstChildElement("param");
		while(!p.isNull())
		{
			QDomElement value = p.firstChildElement("value");
			if(value.isNull())
				throw RuntimeException(QObject::tr("Invalid XML-RPC call"));

			args << parseValue(value);

			p = p.nextSiblingElement("param");
		}
	}
}

