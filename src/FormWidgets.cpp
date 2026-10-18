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

#include "FormWidgets.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QSet>
#include <QSpacerItem>
#include <QToolButton>
#include <QTreeWidget>
#include <QtDebug>

FormWidgets::FormWidgets(const QJsonArray& items, const QJsonObject& values, QWidget* parent)
	: m_values(values)
{
	addItems(items, parent);
}

QJsonValue FormWidgets::initial(const QJsonObject& item) const
{
	QString key = item.value("key").toString();
	QString type = item.value("type").toString();

	if (!key.isEmpty() && m_values.contains(key))
		return m_values.value(key);

	if (type == "checklist")
	{
		QJsonArray checked;

		for (const QJsonValue& row : item.value("rows").toArray())
		{
			if (row.toObject().value("checked").toBool())
				checked << row.toObject().value("value");
		}
		return checked;
	}

	if (item.contains("default"))
		return item.value("default");
	if (type == "checkbox" || type == "group")
		return false;
	if (type == "order")
		return QJsonArray();
	if (type == "list" && !item.value("rows").toArray().isEmpty())
		return item.value("rows").toArray().first().toObject().value("value");
	return QString();
}

// Only a keyed field has a value to read back. The key is the object name, so
// that tests can find the widget.
void FormWidgets::bind(const QString& key, const QString& type, QWidget* widget)
{
	if (key.isEmpty())
		return;
	widget->setObjectName(key);
	m_bound << Bound{key, type, widget};
}

void FormWidgets::addItems(const QJsonArray& items, QWidget* parent)
{
	QGridLayout* layout = new QGridLayout(parent);
	int row = 0;

	for (const QJsonValue& v : items)
	{
		QJsonObject item = v.toObject();
		QString type = item.value("type").toString();
		QString title = item.value("title").toString();
		QString key = item.value("key").toString();
		QJsonValue value = initial(item);

		if (type == "text" || type == "password")
		{
			QLineEdit* edit = new QLineEdit(value.toString(), parent);

			if (type == "password")
				edit->setEchoMode(QLineEdit::Password);

			layout->addWidget(new QLabel(title, parent), row, 0);
			layout->addWidget(edit, row, 1);
			bind(key, type, edit);
		}
		else if (type == "directory")
		{
			QWidget* box = new QWidget(parent);
			QHBoxLayout* hbox = new QHBoxLayout(box);
			QLineEdit* edit = new QLineEdit(value.toString(), box);
			QToolButton* browse = new QToolButton(box);

			hbox->setContentsMargins(0, 0, 0, 0);
			browse->setText("...");
			hbox->addWidget(edit);
			hbox->addWidget(browse);

			QObject::connect(browse, &QToolButton::clicked, edit, [edit] {
				QString dir = QFileDialog::getExistingDirectory(edit->window(),
					QCoreApplication::translate("FormWidgets", "Choose a directory"), edit->text());
				if (!dir.isEmpty())
					edit->setText(dir);
			});

			layout->addWidget(new QLabel(title, parent), row, 0);
			layout->addWidget(box, row, 1);
			bind(key, type, edit);
		}
		else if (type == "checkbox")
		{
			QCheckBox* box = new QCheckBox(title, parent);

			box->setChecked(value.toBool());
			layout->addWidget(box, row, 0, 1, 2);
			bind(key, type, box);
		}
		else if (type == "combo")
		{
			QComboBox* combo = new QComboBox(parent);

			for (const QJsonValue& o : item.value("options").toArray())
			{
				QString optionValue = o.toObject().value("value").toString();
				QString text = o.toObject().value("title").toString();

				combo->addItem(text.isEmpty() ? optionValue : text, optionValue);
				if (optionValue == value.toString())
					combo->setCurrentIndex(combo->count() - 1);
			}

			layout->addWidget(new QLabel(title, parent), row, 0);
			layout->addWidget(combo, row, 1);
			bind(key, type, combo);
		}
		else if (type == "label")
		{
			QLabel* label = new QLabel(title, parent);

			label->setTextFormat(Qt::PlainText); // plugin text is never markup
			label->setWordWrap(true);
			layout->addWidget(label, row, 0, 1, 2);
		}
		else if (type == "list" || type == "checklist")
		{
			QTreeWidget* tree = new QTreeWidget(parent);
			QStringList headers;
			QSet<QString> checked;

			for (const QJsonValue& c : item.value("columns").toArray())
				headers << c.toString();
			for (const QJsonValue& c : value.toArray())
				checked << c.toString();

			tree->setColumnCount(qMax(1, int(headers.size())));
			if (headers.isEmpty())
				tree->setHeaderHidden(true);
			else
				tree->setHeaderLabels(headers);
			tree->setRootIsDecorated(false);

			for (const QJsonValue& r : item.value("rows").toArray())
			{
				QJsonObject rowObject = r.toObject();
				QString rowValue = rowObject.value("value").toString();
				QJsonArray cells = rowObject.value("cells").toArray();
				QTreeWidgetItem* entry = new QTreeWidgetItem(tree);

				if (cells.isEmpty())
					entry->setText(0, rowValue);
				for (int i = 0; i < cells.size(); i++)
					entry->setText(i, cells[i].toString());
				entry->setData(0, Qt::UserRole, rowValue);

				if (type == "checklist")
					entry->setCheckState(0, checked.contains(rowValue) ? Qt::Checked : Qt::Unchecked);
				else if (rowValue == value.toString())
					tree->setCurrentItem(entry);
			}

			if (!title.isEmpty())
				layout->addWidget(new QLabel(title, parent), row++, 0, 1, 2);
			layout->addWidget(tree, row, 0, 1, 2);
			bind(key, type, tree);
		}
		else if (type == "order")
		{
			// A list the user reorders by dragging: what was saved first, then
			// anything the plugin has added since the user last looked.
			QListWidget* list = new QListWidget(parent);
			QMap<QString, QListWidgetItem*> byValue;
			QList<QListWidgetItem*> unordered;

			list->setDragDropMode(QAbstractItemView::InternalMove);

			for (const QJsonValue& o : item.value("options").toArray())
			{
				QString optionValue = o.toObject().value("value").toString();
				QString text = o.toObject().value("title").toString();
				QListWidgetItem* entry = new QListWidgetItem;

				entry->setText(text.isEmpty() ? optionValue : text);
				entry->setData(Qt::UserRole, optionValue);
				byValue[optionValue] = entry;
				unordered << entry;
			}

			for (const QJsonValue& saved : value.toArray())
			{
				QListWidgetItem* entry = byValue.take(saved.toString());

				if (!entry)
					continue;
				list->addItem(entry);
				unordered.removeAll(entry);
			}
			for (QListWidgetItem* entry : unordered)
				list->addItem(entry);

			if (!title.isEmpty())
				layout->addWidget(new QLabel(title, parent), row++, 0, 1, 2);
			layout->addWidget(list, row, 0, 1, 2);
			bind(key, type, list);
		}
		else if (type == "group")
		{
			QGroupBox* box = new QGroupBox(title, parent);

			layout->addWidget(box, row, 0, 1, 2);

			if (!key.isEmpty())
			{
				box->setCheckable(true);
				box->setChecked(value.toBool());
				bind(key, type, box);
			}

			addItems(item.value("items").toArray(), box);
		}
		else
			qDebug() << "Unknown form item type:" << type;

		row++;
	}

	if (!qobject_cast<QGroupBox*>(parent))
		layout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0, 1, 2);
}

QJsonObject FormWidgets::values() const
{
	QJsonObject rv;

	for (const Bound& b : m_bound)
	{
		if (QLineEdit* edit = qobject_cast<QLineEdit*>(b.widget))
			rv[b.key] = edit->text();
		else if (QGroupBox* box = qobject_cast<QGroupBox*>(b.widget))
			rv[b.key] = box->isChecked();
		else if (QCheckBox* box = qobject_cast<QCheckBox*>(b.widget))
			rv[b.key] = box->isChecked();
		else if (QComboBox* combo = qobject_cast<QComboBox*>(b.widget))
			rv[b.key] = combo->itemData(combo->currentIndex()).toString();
		else if (QTreeWidget* tree = qobject_cast<QTreeWidget*>(b.widget))
		{
			if (b.type == "checklist")
			{
				QJsonArray checked;

				for (int i = 0; i < tree->topLevelItemCount(); i++)
				{
					if (tree->topLevelItem(i)->checkState(0) == Qt::Checked)
						checked << tree->topLevelItem(i)->data(0, Qt::UserRole).toString();
				}
				rv[b.key] = checked;
			}
			else
			{
				QTreeWidgetItem* current = tree->currentItem();
				rv[b.key] = current ? current->data(0, Qt::UserRole).toString() : QString();
			}
		}
		else if (QListWidget* list = qobject_cast<QListWidget*>(b.widget))
		{
			QJsonArray order;

			for (int i = 0; i < list->count(); i++)
				order << list->item(i)->data(Qt::UserRole).toString();
			rv[b.key] = order;
		}
	}
	return rv;
}

QList<QPair<QString, QString>> FormWidgets::keys(const QJsonArray& items)
{
	QList<QPair<QString, QString>> rv;

	for (const QJsonValue& v : items)
	{
		QJsonObject item = v.toObject();
		QString key = item.value("key").toString();
		QString type = item.value("type").toString();

		if (!key.isEmpty())
			rv << qMakePair(key, type);
		if (type == "group")
			rv << keys(item.value("items").toArray());
	}
	return rv;
}
