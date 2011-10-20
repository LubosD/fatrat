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

#include "FileSharingSearch.h"
#include "JClass.h"
#include "JSearchPlugin.h"
#include "JArray.h"
#include "RuntimeException.h"
#include <QtDebug>

QList<FileSharingSearch::SearchEngine> FileSharingSearch::m_engines;

extern QList<JObject> g_configListeners;

FileSharingSearch::FileSharingSearch(QWidget *parent) :
    QWidget(parent)
{
	setupUi(this);
}

void FileSharingSearch::globalInit()
{
	try {
		JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
		JClass annotation("info.dolezel.fatrat.plugins.annotations.SearchPluginInfo");

		JSearchPlugin::registerNatives();

		JArgs args;

		args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

		JArray arr = helper.callStatic("findAnnotatedClasses",
						  JSignature().addString().add("java.lang.Class").retA("java.lang.Class"),
						  args).value<JArray>();
		qDebug() << "Found" << arr.size() << "annotated classes (SearchPluginInfo)";

		int classes = arr.size();
		for (int i = 0; i < classes; i++)
		{
			try
			{
				JClass obj = (jobject) arr.getObject(i);
				JObject ann = obj.getAnnotation(annotation);
				QString name = ann.call("name", JSignature().retString()).toString();
				QString clsName = obj.getClassName();
				JObject instance(obj, JSignature());

				qDebug() << "Class name:" << clsName;
				qDebug() << "Name:" << name;

				if (instance.instanceOf("info.dolezel.fatrat.plugins.listeners.ConfigListener"))
					g_configListeners << instance;

				SearchEngine e = { name, clsName, false };
				m_engines << e;
			}
			catch (const RuntimeException& e)
			{
				qDebug() << e.what();
			}
		}
	}
	catch (const RuntimeException& e)
	{
		qDebug() << e.what();
	}
}

void FileSharingSearch::globalExit()
{

}

void FileSharingSearch::addSearchResults(QString fromClass, QList<SearchResult>& res)
{

}

void FileSharingSearch::searchFailed(QString fromClass)
{

}
