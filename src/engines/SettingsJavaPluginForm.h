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

#ifndef SETTINGSJAVAPLUGIN_H
#define SETTINGSJAVAPLUGIN_H
#include <QObject>
#include "WidgetHostChild.h"
#include "ui_SettingsJavaPluginForm.h"
#include "ExtensionDownloadDlg.h"
#include <QNetworkAccessManager>
#include <QList>
#include <QMap>
#include <QDomDocument>

class QNetworkReply;

class SettingsJavaPluginForm : public QObject, Ui_SettingsJavaPluginForm, public WidgetHostChild
{
Q_OBJECT
public:
	SettingsJavaPluginForm(QWidget* me, QObject* parent);
	virtual void load();
	virtual void accepted();
	static WidgetHostChild* create(QWidget* me, QObject* parent) { return new SettingsJavaPluginForm(me, parent); }
private slots:
	void finished(QNetworkReply* reply);
	void finishedDownload(QNetworkReply* reply);
	void uninstall();
	void install();
	void cancelDownload();
private:
	void setError(QString error);
	void loadInstalled();
	void askRestart();
	void downloadNext();
	void setupExtensionPages();
	QWidget* constructPage(QDomDocument& doc);
	void processPageElement(QDomElement& elem, QWidget* widget);
private:
	QNetworkAccessManager* m_network;
	QNetworkReply* m_reply;

	struct Plugin
	{
		QString name, version, desc;
	};
	QList<Plugin> m_availablePlugins;
	QMap<QString,QString> m_installedPlugins;
	QList<QPair<QString,bool> > m_toInstall;
	ExtensionDownloadDlg m_dlgProgress;

	QMap<QWidget*,QString> m_extSettingsWidgets;
};

#endif // SETTINGSJAVAPLUGIN_H
