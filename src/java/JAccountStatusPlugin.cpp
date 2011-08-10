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

#include "JAccountStatusPlugin.h"
#include "JString.h"
#include "JArray.h"
#include <QtDebug>

QList<QPair<QString,QString> > JAccountStatusPlugin::m_listPlugins;

JAccountStatusPlugin::JAccountStatusPlugin(const JClass& cls, QString name)
	: JPlugin(cls, "()V", JArgs()), m_strName(name)
{

}


void JAccountStatusPlugin::registerNatives()
{
	QList<JNativeMethod> natives;

	natives << JNativeMethod("reportAccountBalance",
				 JSignature().add("info.dolezel.fatrat.plugins.AccountStatusPlugin$AccountState").addString(),
				 reportAccountBalance);

	JClass("info.dolezel.fatrat.plugins.AccountStatusPlugin").registerNativeMethods(natives);

	qRegisterMetaType<AccountState>();
}

void JAccountStatusPlugin::reportAccountBalance(JNIEnv*, jobject jthis, jobject jstate, jstring jbal)
{
	JAccountStatusPlugin* This = static_cast<JAccountStatusPlugin*>(getCObject(jthis));
	QString strState = JObject(jstate).call("name", JSignature().retString()).toString();
	QString bal;
	AccountState state = AccountError;

	if (jbal)
		bal = JString(jbal).str();

	if (strState == "AccountGood")
		state = AccountGood;
	else if (strState == "AccountWarning")
		state = AccountWarning;
	else if (strState == "AccountBad")
		state = AccountBad;

	emit This->accountBalanceReceived(state, bal);
}

QList<JAccountStatusPlugin*> JAccountStatusPlugin::createStatusPlugins()
{
	static bool firstTime = true;

	if (firstTime)
	{
		findPlugins();
		firstTime = false;
	}

	QList<JAccountStatusPlugin*> rv;

	for (int i = 0; i < m_listPlugins.size(); i++)
		rv << new JAccountStatusPlugin(m_listPlugins[i].first, m_listPlugins[i].second);

	return rv;
}

void JAccountStatusPlugin::findPlugins()
{
	JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
	JClass annotation("info.dolezel.fatrat.plugins.annotations.AccountStatusPluginInfo");

	QList<QVariant> args;

	args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

	JArray arr = helper.callStatic("findAnnotatedClasses",
					  JSignature().addString().add("java.lang.Class").retA("java.lang.Class"),
					  args).value<JArray>();
	qDebug() << "Found" << arr.size() << "annotated classes (AccountStatusPluginInfo)";

	int classes = arr.size();
	for (int i = 0; i < classes; i++)
	{
		try
		{
			JClass obj = (jobject) arr.getObject(i);
			JObject ann = obj.getAnnotation(annotation);
			QString name = ann.call("name", JSignature().retString()).toString();

			m_listPlugins << QPair<QString,QString>(obj.getClassName(), name);
		}
		catch (...)
		{
		}
	}
}
