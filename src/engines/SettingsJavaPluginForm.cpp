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

#include "SettingsJavaPluginForm.h"
#include "java/JVM.h"
#include "fatrat.h"
#include <QNetworkReply>
#include <QMessageBox>
#include <QDir>

extern const char* USER_PROFILE_PATH;
static const char* UPDATE_URL = "http://fatrat.dolezel.info/update/plugins/Index";

SettingsJavaPluginForm::SettingsJavaPluginForm(QWidget* me, QObject* parent) : QObject(parent)
{
	setupUi(me);
	m_network = new QNetworkAccessManager(this);
	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
	connect(pushUninstall, SIGNAL(clicked()), this, SLOT(uninstall()));
	connect(pushInstall, SIGNAL(clicked()), this, SLOT(install()));
}

void SettingsJavaPluginForm::load()
{
	QMap<QString,QString> packages = JVM::instance()->getPackageVersions();
	m_network->get(QNetworkRequest(QUrl(UPDATE_URL)));

	m_installedPlugins = JVM::instance()->getPackageVersions();

	loadInstalled();
	treeInstalled->header()->resizeSection(0, 200);
	treeAvailable->header()->resizeSection(0, 200);
	treeUpdates->header()->resizeSection(0, 200);
}

void SettingsJavaPluginForm::accepted()
{

}

void SettingsJavaPluginForm::loadInstalled()
{
	treeInstalled->clear();
	for (QMap<QString,QString>::iterator it = m_installedPlugins.begin(); it != m_installedPlugins.end(); it++)
	{
		QString name = it.key();
		name.remove(".jar");

		if (name == "fatrat-jplugins")
			continue;

		QTreeWidgetItem* i = new QTreeWidgetItem(treeInstalled, QStringList() << name << it.value());
		i->setFlags(i->flags() | Qt::ItemIsUserCheckable);
		i->setCheckState(0, Qt::Unchecked);
		treeInstalled->addTopLevelItem(i);
	}
}

void SettingsJavaPluginForm::setError(QString error)
{
	treeAvailable->addTopLevelItem(new QTreeWidgetItem(treeAvailable, QStringList(error)));
	treeUpdates->addTopLevelItem(new QTreeWidgetItem(treeAvailable, QStringList(error)));
}

void SettingsJavaPluginForm::finished(QNetworkReply* reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		setError(tr("Failed to download the plugin index file: %1").arg(reply->errorString()));
		return;
	}

	bool versionChecked = false;
	bool versionOK = false;

	while (!reply->atEnd())
	{
		QString line = reply->readLine().trimmed();
		if (line.isEmpty() && versionChecked)
			break;
		if (line.isEmpty() && !versionChecked)
		{
			if (!versionOK)
			{
				setError(tr("Your version of FatRat is no longer supported by the update site."));
				return;
			}
			else
				versionChecked = true;
		}

		if (line.startsWith("Version: "))
		{
			if (line.mid(9) == VERSION)
				versionOK = true;
		}
		else
		{
			QStringList parts = line.split('\t');
			if (parts.count() < 3)
				continue;

			Plugin plugin;
			plugin.name = parts[0];
			plugin.version = parts[1];
			plugin.desc = parts[2];

			m_availablePlugins << plugin;
		}
	}

	for (int i = 0; i < m_availablePlugins.size(); i++)
	{
		const Plugin& pl = m_availablePlugins[i];
		QString fullName = pl.name + ".jar";

		if (m_installedPlugins.contains(fullName))
		{
			if (pl.version > m_installedPlugins[fullName])
			{
				QTreeWidgetItem* i = new QTreeWidgetItem(treeUpdates, QStringList() << pl.name << pl.version);

				if (pl.name != "fatrat-jplugins")
				{
					i->setFlags(i->flags() | Qt::ItemIsUserCheckable);
					i->setCheckState(0, Qt::Unchecked);
				}
				else
				{
					i->setCheckState(0, Qt::Checked);
					i->setFlags( i->flags() & ~(Qt::ItemIsUserCheckable|Qt::ItemIsEnabled));
				}
				treeUpdates->addTopLevelItem(i);
			}
		}
		else
		{
			QTreeWidgetItem* i = new QTreeWidgetItem(treeAvailable, QStringList() << pl.name << pl.desc);
			i->setFlags(i->flags() | Qt::ItemIsUserCheckable);
			i->setCheckState(0, Qt::Unchecked);
			treeAvailable->addTopLevelItem(i);
		}
	}

	toolBox->setItemText(0, QString("%1 (%2)").arg(toolBox->itemText(0)).arg(treeAvailable->topLevelItemCount()));
	toolBox->setItemText(1, QString("%1 (%2)").arg(toolBox->itemText(1)).arg(treeUpdates->topLevelItemCount()));
}

void SettingsJavaPluginForm::uninstall()
{
	if (QMessageBox::warning(getMainWindow(), "FatRat", tr("Do you really wish to uninstall the selected plugins?"), QMessageBox::Yes|QMessageBox::Cancel) != QMessageBox::Yes)
	{
		return;
	}

	QString baseDir = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/data/java/";
	QDir dir(baseDir);
	int numRemoved = 0;

	for (int i = 0; i < treeInstalled->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* item = treeInstalled->topLevelItem(i);
		if (item->checkState(0) != Qt::Checked)
			continue;
		QString fileName = item->text(0) + ".jar";

		if (!dir.remove(fileName))
			QMessageBox::critical(getMainWindow(), "FatRat", tr("Failed to remove \"%1\", check the file permissions.").arg(fileName));
		numRemoved++;
	}

	if (numRemoved)
		askRestart();
}

void SettingsJavaPluginForm::askRestart()
{
	QMessageBox box(QMessageBox::Warning, "FatRat", tr("For the changes to take effect, FatRat needs to be restarted."),
			QMessageBox::NoButton, getMainWindow());
	QPushButton* now = box.addButton(tr("Restart now"), QMessageBox::AcceptRole);
	box.addButton(tr("Restart later"), QMessageBox::RejectRole);
	box.exec();

	if (box.clickedButton() == now)
		restartApplication();
}

void SettingsJavaPluginForm::install()
{

}
