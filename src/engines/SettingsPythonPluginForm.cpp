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

#include "SettingsPythonPluginForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QApplication>
#include <QMessageBox>
#include <QtDebug>

#include "Settings.h"
#include "fatrat.h"
#include "python/PluginRegistry.h"
#include "python/PluginRepository.h"

SettingsPythonPluginForm::SettingsPythonPluginForm(QWidget* me, QObject* parent)
	: QObject(parent)
{
	setupUi(me);

	connect(pushRemove, SIGNAL(clicked()), this, SLOT(removeSelected()));
	connect(pushAddRepository, SIGNAL(clicked()), this, SLOT(addRepository()));
	connect(pushAddDirectory, SIGNAL(clicked()), this, SLOT(addRepositoryDirectory()));
	connect(pushRemoveRepository, SIGNAL(clicked()), this, SLOT(removeRepository()));
	connect(pushRefresh, SIGNAL(clicked()), this, SLOT(refreshRepositories()));
	connect(pushInstall, SIGNAL(clicked()), this, SLOT(installSelected()));
	connect(lineRepository, SIGNAL(returnPressed()), this, SLOT(addRepository()));
}

void SettingsPythonPluginForm::load()
{
	loadRepositories();
	loadAvailable();
	loadInstalled();
	buildSettingsPages();
}

void SettingsPythonPluginForm::loadRepositories()
{
	treeRepositories->clear();
	treeRepositories->header()->resizeSection(0, 160);
	treeRepositories->header()->resizeSection(1, 260);

	foreach (const PluginRepository::Repository& repository, PluginRepository::repositories())
	{
		QTreeWidgetItem* item = new QTreeWidgetItem(treeRepositories);

		item->setText(0, repository.name.isEmpty() ? repository.id : repository.name);
		item->setText(1, repository.source);
		item->setText(2, repository.local ? tr("local directory") : repository.revision.left(10));
		item->setData(0, Qt::UserRole, repository.id);

		treeRepositories->addTopLevelItem(item);
	}
}

void SettingsPythonPluginForm::loadAvailable()
{
	treeAvailable->clear();
	treeAvailable->header()->resizeSection(0, 140);

	foreach (const PluginRepository::Available& entry, PluginRepository::available())
	{
		QTreeWidgetItem* item = new QTreeWidgetItem(treeAvailable);

		item->setText(0, entry.plugin.name);
		item->setText(1, entry.plugin.version);
		item->setText(2, entry.installedVersion);
		item->setText(3, entry.repository);
		item->setText(4, entry.plugin.description);
		item->setData(0, Qt::UserRole, entry.plugin.name);
		item->setData(1, Qt::UserRole, entry.repository);

		// An update available is the one thing worth pointing at.
		if (!entry.installedVersion.isEmpty() && entry.installedVersion != entry.plugin.version)
		{
			QFont font = item->font(0);

			font.setBold(true);
			item->setFont(0, font);
			item->setFont(1, font);
		}

		treeAvailable->addTopLevelItem(item);
	}

	tabWidget->setTabText(1, tr("Available (%1)").arg(treeAvailable->topLevelItemCount()));
}

void SettingsPythonPluginForm::addRepository()
{
	addRepositorySource(lineRepository->text().trimmed());
}

void SettingsPythonPluginForm::addRepositoryDirectory()
{
	QString directory = QFileDialog::getExistingDirectory(getMainWindow(),
		tr("Choose a plugin repository directory"));

	if (!directory.isEmpty())
		addRepositorySource(directory);
}

/* The moment of trust: everything this repository offers will run as the
 * user. Say so before fetching anything, and show exactly what is being
 * added.
 */
void SettingsPythonPluginForm::addRepositorySource(const QString& source)
{
	if (source.isEmpty())
		return;

	if (QMessageBox::warning(getMainWindow(), "FatRat",
		tr("Add this plugin repository?\n\n%1\n\n"
		   "Plugins you install from it run with your full user privileges, "
		   "like anything else you install from a git repository. Only add "
		   "repositories you trust.").arg(source),
		QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	QString error;

	if (!PluginRepository::addRepository(source, &error))
	{
		QMessageBox::critical(getMainWindow(), "FatRat",
			tr("Could not add the repository: %1").arg(error));
		return;
	}

	lineRepository->clear();
	loadRepositories();
	loadAvailable();
}

void SettingsPythonPluginForm::removeRepository()
{
	QTreeWidgetItem* item = treeRepositories->currentItem();

	if (!item)
		return;

	QString id = item->data(0, Qt::UserRole).toString();
	QString error;

	if (QMessageBox::question(getMainWindow(), "FatRat",
		tr("Forget the repository \"%1\"? Plugins already installed from it stay installed.")
			.arg(item->text(0)),
		QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	if (!PluginRepository::removeRepository(id, &error))
	{
		QMessageBox::critical(getMainWindow(), "FatRat", error);
		return;
	}

	loadRepositories();
	loadAvailable();
}

void SettingsPythonPluginForm::refreshRepositories()
{
	QString error;

	// Fetching can take a moment over a slow link; the dialog is modal
	// anyway, so just say that something is happening.
	QApplication::setOverrideCursor(Qt::WaitCursor);
	bool ok = PluginRepository::refresh(QString(), &error);
	QApplication::restoreOverrideCursor();

	if (!ok)
		QMessageBox::critical(getMainWindow(), "FatRat",
			tr("Could not refresh the repositories: %1").arg(error));

	loadRepositories();
	loadAvailable();
}

void SettingsPythonPluginForm::installSelected()
{
	QTreeWidgetItem* item = treeAvailable->currentItem();

	if (!item)
		return;

	QString plugin = item->data(0, Qt::UserRole).toString();
	QString repository = item->data(1, Qt::UserRole).toString();
	QString error;

	QStringList dependencies = PluginRepository::dependenciesOf(repository, plugin);
	bool withDependencies = false;

	if (!dependencies.isEmpty())
	{
		// Dependencies come from PyPI, not from the repository the user
		// chose, so they get their own question naming every package.
		if (QMessageBox::warning(getMainWindow(), "FatRat",
			tr("%1 needs these packages from the Python Package Index:\n\n%2\n\n"
			   "They will be downloaded with pip and installed for this plugin only. "
			   "Install them?").arg(plugin).arg(dependencies.join(QLatin1String("\n"))),
			QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		withDependencies = true;
	}

	if (!PluginRepository::install(repository, plugin, &error, withDependencies))
	{
		QMessageBox::critical(getMainWindow(), "FatRat",
			tr("Could not install %1: %2").arg(plugin).arg(error));
		return;
	}

	loadAvailable();
	loadInstalled();

	QMessageBox::information(getMainWindow(), "FatRat",
		tr("%1 is installed. Restart FatRat to load it.").arg(plugin));
}

void SettingsPythonPluginForm::loadInstalled()
{
	treeInstalled->clear();
	treeInstalled->header()->resizeSection(0, 160);

	foreach (const PluginRegistry::Manifest& manifest, PluginRegistry::installed())
	{
		QTreeWidgetItem* item = new QTreeWidgetItem(treeInstalled);

		if (manifest.valid)
		{
			item->setText(0, manifest.name);
			item->setText(1, manifest.version);
			item->setText(2, manifest.description);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
			item->setCheckState(0, PluginRegistry::isPluginEnabled(manifest.name)
					    ? Qt::Checked : Qt::Unchecked);
			item->setData(0, Qt::UserRole, manifest.name);
			item->setData(1, Qt::UserRole, manifest.directory);
		}
		else
		{
			// Show the broken ones too: a plugin that silently does not
			// appear is the worst kind of plugin.
			item->setText(0, QDir(manifest.directory).dirName());
			item->setText(2, manifest.error);
			item->setDisabled(true);
		}

		treeInstalled->addTopLevelItem(item);
	}
}

void SettingsPythonPluginForm::buildSettingsPages()
{
	m_boundWidgets.clear();
	comboSettingsPages->clear();

	while (stackedWidget->count())
	{
		QWidget* page = stackedWidget->widget(0);

		stackedWidget->removeWidget(page);
		delete page;
	}

	foreach (const PluginRegistry::Manifest& manifest, PluginRegistry::installed())
	{
		if (!manifest.valid || manifest.config.isEmpty())
			continue;
		if (!PluginRegistry::isPluginEnabled(manifest.name))
			continue;

		comboSettingsPages->addItem(manifest.description.isEmpty()
			? manifest.name
			: QString("%1 - %2").arg(manifest.name).arg(manifest.description));
		stackedWidget->addWidget(buildPage(manifest.config));
	}

	if (!stackedWidget->count())
	{
		QLabel* empty = new QLabel(tr("No installed plugin has settings."), stackedWidget);

		empty->setAlignment(Qt::AlignCenter);
		comboSettingsPages->addItem(tr("(none)"));
		stackedWidget->addWidget(empty);
	}

	stackedWidget->setCurrentIndex(0);
}

QWidget* SettingsPythonPluginForm::buildPage(const QJsonArray& items)
{
	QWidget* page = new QWidget(stackedWidget);

	addItems(items, page);

	return page;
}

/* The widget vocabulary a plugin may use. It is deliberately the same set the
 * Java extensions had in their XML dialogs -- text, password, checkbox, combo,
 * group, order -- since that proved to be what plugins actually need.
 */
void SettingsPythonPluginForm::addItems(const QJsonArray& items, QWidget* parent)
{
	QGridLayout* layout = new QGridLayout(parent);
	int row = 0;

	for (QJsonArray::const_iterator it = items.begin(); it != items.end(); it++, row++)
	{
		QJsonObject item = (*it).toObject();
		QString type = item.value(QLatin1String("type")).toString();
		QString title = item.value(QLatin1String("title")).toString();
		QString key = item.value(QLatin1String("key")).toString();

		if (type == QLatin1String("text") || type == QLatin1String("password"))
		{
			QLabel* label = new QLabel(title, parent);
			QLineEdit* edit = new QLineEdit(parent);

			if (type == QLatin1String("password"))
				edit->setEchoMode(QLineEdit::Password);

			edit->setText(getSettingsValue(key).toString());

			layout->addWidget(label, row, 0);
			layout->addWidget(edit, row, 1);

			m_boundWidgets[edit] = key;
		}
		else if (type == QLatin1String("checkbox"))
		{
			QCheckBox* box = new QCheckBox(title, parent);

			box->setChecked(getSettingsValue(key, false).toBool());
			layout->addWidget(box, row, 0, 1, 2);

			m_boundWidgets[box] = key;
		}
		else if (type == QLatin1String("combo"))
		{
			QLabel* label = new QLabel(title, parent);
			QComboBox* combo = new QComboBox(parent);
			QJsonArray options = item.value(QLatin1String("options")).toArray();
			QString current = getSettingsValue(key).toString();

			for (QJsonArray::const_iterator o = options.begin(); o != options.end(); o++)
			{
				QJsonObject option = (*o).toObject();
				QString value = option.value(QLatin1String("value")).toString();
				QString text = option.value(QLatin1String("title")).toString();

				combo->addItem(text.isEmpty() ? value : text, value);

				if (value == current)
					combo->setCurrentIndex(combo->count() - 1);
			}

			layout->addWidget(label, row, 0);
			layout->addWidget(combo, row, 1);

			m_boundWidgets[combo] = key;
		}
		else if (type == QLatin1String("order"))
		{
			// A list the user reorders by dragging; the plugin reads it
			// back with fatrat.settings.array().
			QListWidget* list = new QListWidget(parent);
			QJsonArray options = item.value(QLatin1String("options")).toArray();
			QMap<QString, QListWidgetItem*> byValue;
			QList<QListWidgetItem*> unordered;

			list->setDragDropMode(QAbstractItemView::InternalMove);

			for (QJsonArray::const_iterator o = options.begin(); o != options.end(); o++)
			{
				QJsonObject option = (*o).toObject();
				QString value = option.value(QLatin1String("value")).toString();
				QString text = option.value(QLatin1String("title")).toString();
				QListWidgetItem* entry = new QListWidgetItem;

				entry->setText(text.isEmpty() ? value : text);
				entry->setData(Qt::UserRole, value);

				byValue[value] = entry;
				unordered << entry;
			}

			// The saved order first, then anything the plugin has added
			// since the user last looked.
			QList<QMap<QString, QVariant> > saved = getSettingsArray(key);

			for (int i = 0; i < saved.size(); i++)
			{
				QString value = saved[i].value(QLatin1String("item")).toString();

				if (!byValue.contains(value))
					continue;

				QListWidgetItem* entry = byValue.take(value);

				list->addItem(entry);
				unordered.removeAll(entry);
			}

			foreach (QListWidgetItem* entry, unordered)
				list->addItem(entry);

			if (!title.isEmpty())
			{
				layout->addWidget(new QLabel(title, parent), row, 0, 1, 2);
				row++;
			}

			layout->addWidget(list, row, 0, 1, 2);

			m_boundWidgets[list] = key;
		}
		else if (type == QLatin1String("group"))
		{
			QGroupBox* box = new QGroupBox(title, parent);

			layout->addWidget(box, row, 0, 1, 2);

			if (!key.isEmpty())
			{
				box->setCheckable(true);
				box->setChecked(getSettingsValue(key, false).toBool());
				m_boundWidgets[box] = key;
			}

			addItems(item.value(QLatin1String("items")).toArray(), box);
		}
		else
			qDebug() << "Unknown plugin config item type:" << type;
	}

	if (!dynamic_cast<QGroupBox*>(parent))
		layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0, 1, 2);

	parent->setLayout(layout);
}

void SettingsPythonPluginForm::accepted()
{
	for (QMap<QWidget*, QString>::iterator it = m_boundWidgets.begin(); it != m_boundWidgets.end(); it++)
	{
		QWidget* widget = it.key();
		const QString& key = it.value();

		if (QLineEdit* edit = dynamic_cast<QLineEdit*>(widget))
			setSettingsValue(key, edit->text());
		else if (QGroupBox* box = dynamic_cast<QGroupBox*>(widget))
			setSettingsValue(key, box->isChecked());
		else if (QCheckBox* box = dynamic_cast<QCheckBox*>(widget))
			setSettingsValue(key, box->isChecked());
		else if (QComboBox* combo = dynamic_cast<QComboBox*>(widget))
			setSettingsValue(key, combo->itemData(combo->currentIndex()));
		else if (QListWidget* list = dynamic_cast<QListWidget*>(widget))
		{
			QList<QMap<QString, QVariant> > values;

			for (int i = 0; i < list->count(); i++)
			{
				QMap<QString, QVariant> entry;

				entry[QLatin1String("item")] = list->item(i)->data(Qt::UserRole).toString();
				values << entry;
			}

			setSettingsArray(key, values);
		}
	}

	// Enabling or disabling a plugin only takes effect on the next start:
	// engines are registered once, at startup.
	for (int i = 0; i < treeInstalled->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* item = treeInstalled->topLevelItem(i);
		QString name = item->data(0, Qt::UserRole).toString();

		if (name.isEmpty())
			continue;

		PluginRegistry::setPluginEnabled(name, item->checkState(0) == Qt::Checked);
	}

	PluginRegistry::notifyConfigSaved();
}

void SettingsPythonPluginForm::removeSelected()
{
	QTreeWidgetItem* item = treeInstalled->currentItem();

	if (!item)
		return;

	QString name = item->data(0, Qt::UserRole).toString();
	QString directory = item->data(1, Qt::UserRole).toString();

	if (directory.isEmpty())
		directory = item->text(0);

	if (QMessageBox::warning(getMainWindow(), "FatRat",
				 tr("Remove the plugin \"%1\"? Its files will be deleted.").arg(name.isEmpty() ? item->text(0) : name),
				 QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	QString error;

	if (!PluginRepository::uninstall(name.isEmpty() ? item->text(0) : name, &error))
	{
		QMessageBox::critical(getMainWindow(), "FatRat",
				      tr("Failed to remove \"%1\": %2").arg(directory).arg(error));
		return;
	}

	loadAvailable();
	loadInstalled();
	buildSettingsPages();
}
