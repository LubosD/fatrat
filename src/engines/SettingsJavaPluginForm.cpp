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
#include "java/JClass.h"
#include "fatrat.h"
#include "config.h"
#include "engines/JavaDownload.h"
#include "Settings.h"
#include <QNetworkReply>
#include <QMessageBox>
#include <QDir>
#include <QtDebug>
#include <QGroupBox>
#include <QListWidget>

static const QLatin1String UPDATE_URL = QLatin1String("http://fatrat.dolezel.info/update/plugins/");

SettingsJavaPluginForm::SettingsJavaPluginForm(QWidget* me, QObject* parent)
	: QObject(parent), m_reply(0), m_dlgProgress(me)
{
	setupUi(me);
	m_network = new QNetworkAccessManager(this);

	connect(pushUninstall, SIGNAL(clicked()), this, SLOT(uninstall()));
	connect(pushInstall, SIGNAL(clicked()), this, SLOT(install()));
	connect(&m_dlgProgress, SIGNAL(rejected()), this, SLOT(cancelDownload()));

	setupExtensionPages();
}

void SettingsJavaPluginForm::load()
{
	treeInstalled->clear();
	treeAvailable->clear();
	treeUpdates->clear();
	m_availablePlugins.clear();

	QMap<QString,QString> packages = JVM::instance()->getPackageVersions();

	connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
	m_network->get(QNetworkRequest(QUrl(UPDATE_URL + "Index")));

	m_installedPlugins = JVM::instance()->getPackageVersions();

	loadInstalled();
	treeInstalled->header()->resizeSection(0, 200);
	treeAvailable->header()->resizeSection(0, 200);
	treeUpdates->header()->resizeSection(0, 200);
}

void SettingsJavaPluginForm::accepted()
{
	for (QMap<QWidget*,QString>::iterator it = m_extSettingsWidgets.begin(); it != m_extSettingsWidgets.end(); it++)
	{
		if (QLineEdit* edit = dynamic_cast<QLineEdit*>(it.key()))
		{
			setSettingsValue(it.value(), edit->text());
		}
		else if (QListWidget* list = dynamic_cast<QListWidget*>(it.key()))
		{
			if (list->dragDropMode() == QAbstractItemView::InternalMove) // order item
			{
				QList<QMap<QString, QVariant> > slist;

				for (int i = 0; i < list->count(); i++)
				{
					QListWidgetItem* item = list->item(i);
					QMap<QString, QVariant> map;

					map["item"] = item->data(Qt::UserRole).toString();
					slist << map;
				}

				setSettingsArray(it.value(), slist);
			}
		}
	}
}

void SettingsJavaPluginForm::loadInstalled()
{
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
	m_reply = 0;

	disconnect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));

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

	toolBox->setItemText(0, tr("Available extensions (%2)").arg(treeAvailable->topLevelItemCount()));
	toolBox->setItemText(1, tr("Updates (%2)").arg(treeUpdates->topLevelItemCount()));
}

void SettingsJavaPluginForm::uninstall()
{
	if (QMessageBox::warning(getMainWindow(), "FatRat", tr("Do you really wish to uninstall the selected plugins?"), QMessageBox::Yes|QMessageBox::Cancel) != QMessageBox::Yes)
	{
		return;
	}

	QString baseDir = QDir::homePath() + USER_PROFILE_PATH "/data/java/";
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
	else
		load();
}

void SettingsJavaPluginForm::install()
{
	m_toInstall.clear();

	for (int i = 0; i < treeAvailable->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* item = treeAvailable->topLevelItem(i);
		if (item->checkState(0) == Qt::Checked)
			m_toInstall << QPair<QString,bool>(item->text(0), false);
	}
	for (int i = 0; i < treeUpdates->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* item = treeUpdates->topLevelItem(i);
		if (item->checkState(0) == Qt::Checked)
			m_toInstall << QPair<QString,bool>(item->text(0), true);
	}

	if (!m_toInstall.isEmpty())
	{
		connect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finishedDownload(QNetworkReply*)));
		m_dlgProgress.setNumSteps(m_toInstall.size());
		m_dlgProgress.show();

		downloadNext();
	}
}

void SettingsJavaPluginForm::finishedDownload(QNetworkReply* reply)
{
	reply->deleteLater();
	if (m_toInstall.isEmpty())
	{
		m_dlgProgress.hide();
		return;
	}

	QPair<QString,bool> item = m_toInstall.takeFirst();

	if (reply->error() != QNetworkReply::NoError)
	{
		QMessageBox::critical(getMainWindow(), "FatRat", tr("Failed to download an extension: %1").arg(reply->errorString()));
		m_dlgProgress.hide();
		return;
	}

	QDir::home().mkpath(USER_PROFILE_PATH "/data/java/");

	QString baseDir = QDir::homePath() + USER_PROFILE_PATH "/data/java/";
	QDir dir(baseDir);

	QString fullPath = dir.filePath(item.first+".jar");
	QFile file(fullPath);

	if (!file.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(getMainWindow(), "FatRat", tr("Failed to write a file: %1").arg(file.errorString()));
		m_dlgProgress.hide();
		return;
	}

	m_dlgProgress.incStep();
	file.write(reply->readAll());
	file.close();

	if (!item.second)
	{
		// load it
		JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
		helper.callStatic("loadPackage", JSignature().addString(), JArgs() << fullPath);
	}

	downloadNext();
}

void SettingsJavaPluginForm::downloadNext()
{
	if (m_toInstall.isEmpty())
	{
		disconnect(m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(finishedDownload(QNetworkReply*)));
		m_dlgProgress.hide();
		askRestart();
	}
	else if (m_dlgProgress.isVisible())
	{
		QString name = m_toInstall[0].first;
		QString url = UPDATE_URL + name + ".jar";

		m_reply = m_network->get(QNetworkRequest(url));
	}
}

void SettingsJavaPluginForm::cancelDownload()
{
	m_reply->abort();
	m_toInstall.clear();
}

void SettingsJavaPluginForm::setupExtensionPages()
{
	JClass nativeHelpers("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
	QStringList dlgs = JavaDownload::getConfigDialogs();

	foreach (QString dlg, dlgs)
	{
		QString xml = nativeHelpers.callStatic("loadDialogFile", JSignature().addString().retString(), JArgs() << dlg).toString();

		if (xml.isEmpty())
			continue;

		QDomDocument doc;
		QDomElement root;

		if (!doc.setContent(xml))
			continue;

		root = doc.documentElement();

		comboSettingsPages->addItem(root.attribute("title"));
		stackedWidget->addWidget(constructPage(doc));
	}

	stackedWidget->setCurrentIndex(0);
}

QWidget* SettingsJavaPluginForm::constructPage(QDomDocument& doc)
{
	QWidget* rootWidget = new QWidget(stackedWidget);
	QDomElement root = doc.documentElement();

	processPageElement(root, rootWidget);

	rootWidget->show();
	rootWidget->resize(200,200);
	//return new QLineEdit;
	return rootWidget;
}

void SettingsJavaPluginForm::processPageElement(QDomElement& elem, QWidget* widget)
{
	QGridLayout* layout = new QGridLayout(widget);
	int row = 0;

	QDomNode child = elem.firstChild();
	while (!child.isNull())
	{
		if (!child.isElement())
		{
			child = child.nextSibling();
			continue;
		}

		QDomElement obj = child.toElement();
		QString objType = obj.tagName();
		QString title = obj.attribute("title");

		qDebug() << objType;

		if (objType == "text" || objType == "password")
		{
			QLabel* label = new QLabel(widget);
			QLineEdit* edit = new QLineEdit(widget);
			QString name = obj.attribute("id");

			label->setText(title);

			if (objType == "password")
				edit->setEchoMode(QLineEdit::Password);

			edit->setText(getSettingsValue(name).toString());

			layout->addWidget(label, row, 0);
			layout->addWidget(edit, row, 1);
			label->show();
			edit->show();

			m_extSettingsWidgets[edit] = name;
		}
		else if (objType == "group")
		{
			QGroupBox* box = new QGroupBox(widget);
			box->setTitle(title);
			layout->addWidget(box, row, 0, 1, 2);

			if (obj.hasAttribute("id")) {
				QString name = obj.attribute("id");
				box->setCheckable(true);
				box->setChecked(getSettingsValue(name).toBool());
				m_extSettingsWidgets[box] = name;
			}
			processPageElement(obj, box);
		}
		else if (objType == "order")
		{
			QListWidget* list = new QListWidget(widget);
			list->setDragDropMode(QAbstractItemView::InternalMove);

			QString id = obj.attribute("id");
			QDomElement option = obj.firstChildElement("option");
			QMap<QString, QListWidgetItem*> items;
			QList<QListWidgetItem*> items2;

			while (!option.isNull())
			{
				QString text, value;
				QListWidgetItem* item = new QListWidgetItem;

				text = option.text();
				if (elem.hasAttribute("value"))
					value = option.attribute("value");
				else
					value = text;

				item->setText(text);
				item->setData(Qt::UserRole, value);

				items[value] = item;
				items2 << item;

				option = option.nextSiblingElement("option");
			}

			QList<QMap<QString, QVariant> > cur = getSettingsArray(id);
			for (int i = 0; i < cur.size(); i++)
			{
				QString val = cur[i]["item"].toString();
				qDebug() << "Val:" << val;
				if (items.contains(val))
				{
					qDebug() << "Adding a" << val;
					QMap<QString, QListWidgetItem*>::iterator it = items.find(val);
					list->addItem(it.value());
					items2.removeAll(it.value());
					items.erase(it);
				}
			}

			foreach (QListWidgetItem* item, items2)
				list->addItem(item);
			layout->addWidget(list, row, 0, 1, 2);

			m_extSettingsWidgets[list] = id;
		}

		child = child.nextSibling();
		row++;
	}

	if (!dynamic_cast<QGroupBox*>(widget))
	{
		QSpacerItem* spacerItem = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
		layout->addItem(spacerItem, row, 0, 1, 2);
	}

	widget->setLayout(layout);
	layout->update();
}
