/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef SETTINGSPYTHONPLUGINFORM_H
#define SETTINGSPYTHONPLUGINFORM_H

#include "config.h"
#ifndef WITH_PYPLUGINS
#	error This file is not supposed to be included!
#endif

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QWidget>
#include <memory>
#include <vector>

#include "WidgetHostChild.h"
#include "ui_SettingsPythonPluginForm.h"

class FormWidgets;

/* Settings -> Extensions.
 *
 * Lists what is installed and renders each plugin's settings page from the
 * `config` array in its plugin.json -- so a plugin declares its settings as
 * data once, and every FatRat interface can show them.
 */
class SettingsPythonPluginForm : public QObject, public WidgetHostChild, Ui_SettingsPythonPluginForm
{
Q_OBJECT
public:
	SettingsPythonPluginForm(QWidget* me, QObject* parent);
	~SettingsPythonPluginForm();

	static WidgetHostChild* create(QWidget* me, QObject* parent)
	{
		return new SettingsPythonPluginForm(me, parent);
	}

	virtual void load();
	virtual void accepted();
private slots:
	void removeSelected();
	void addRepository();
	void addRepositoryDirectory();
	void removeRepository();
	void refreshRepositories();
	void installSelected();
private:
	void addRepositorySource(const QString& source);
	void loadRepositories();
	void loadAvailable();
	void loadInstalled();
	void buildSettingsPages();
	QWidget* buildPage(const QJsonArray& items);

	// One per plugin settings page, in the order of stackedWidget.
	struct Page
	{
		QJsonArray items; // what the form was built from, for saving
		std::unique_ptr<FormWidgets> form;
	};
	std::vector<Page> m_pages;
};

#endif // SETTINGSPYTHONPLUGINFORM_H
