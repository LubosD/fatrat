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

#include "JavaAccountStatusWidget.h"
#include <QtDebug>

JavaAccountStatusWidget::JavaAccountStatusWidget(QWidget* parent)
	: QFrame(parent)
{
	setupUi(this);

	QPalette p = treeWidget->palette();
	QColor c = p.color(QPalette::Window);
	p.setColor(QPalette::Base, c);
	treeWidget->setPalette(p);

	m_plugins = JAccountStatusPlugin::createStatusPlugins();

	treeWidget->header()->resizeSection(0, 25);

	foreach (JAccountStatusPlugin* p, m_plugins)
	{
		if (p->queryAccountBalance())
		{
			qDebug() << "Adding" << p->name();
			QString text = QString("<b>%1:</b> %2").arg(p->name()).arg(tr("Requesting..."));
			QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget);
			QLabel* label = new QLabel(treeWidget);

			label->setText(text);
			item->setIcon(0, QIcon(":/states/waiting.png"));
			treeWidget->setItemWidget(item, 1, label);

			m_items[p->name()] = item;
			connect(p, SIGNAL(accountBalanceReceived(JAccountStatusPlugin::AccountState,QString)), this, SLOT(accountBalanceReceived(JAccountStatusPlugin::AccountState,QString)));
		}
	}
}

JavaAccountStatusWidget::~JavaAccountStatusWidget()
{
	qDeleteAll(m_plugins);
}

void JavaAccountStatusWidget::accountBalanceReceived(JAccountStatusPlugin::AccountState state, QString bal)
{
	JAccountStatusPlugin* p = static_cast<JAccountStatusPlugin*>(sender());
	QTreeWidgetItem* item = m_items[p->name()];
	QLabel* label = static_cast<QLabel*>(treeWidget->itemWidget(m_items[p->name()], 1));

	label->setText(QString("<b>%1:</b> %2").arg(p->name()).arg(bal));

	const char* path;

	switch (state)
	{
	case JAccountStatusPlugin::AccountBad:
		path = ":/acc_state/red.png";
		break;
	case JAccountStatusPlugin::AccountWarning:
		path = ":/acc_state/yellow.png";
		break;
	case JAccountStatusPlugin::AccountGood:
		path = ":/acc_state/green.png";
		break;
	default:
		path = ":/states/failed.png";
		break;
	}

	item->setIcon(0, QIcon(path));
}
