/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#ifndef ABOUTDLG_H
#define ABOUTDLG_H
#include "config.h"

#include <QDialog>
#include <QFile>
#include <QHeaderView>
#include "fatrat.h"
#include "ui_AboutDlg.h"

class AboutDlg : public QDialog, Ui_AboutDlg
{
Q_OBJECT
public:
	AboutDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		
		QTableWidgetItem* first;
		QStringList items;
		items << "FatRat" << tr("License") << tr("Translations") << tr("3rd parties") << tr("Features");
		
		for(int i=0;i<items.size();i++)
		{
			QTableWidgetItem* item = new QTableWidgetItem(items[i]);
			item->setTextAlignment(Qt::AlignCenter);
			tableMenu->setItem(i, 0, item);
			
			if(!i) first = item;
		}
		
		tableMenu->setCurrentItem(first);
		tableMenu->horizontalHeader()->hide();
		tableMenu->verticalHeader()->hide();
		tableMenu->setColumnWidth(0, 116);
		
		labelVersion->setText(tr("Version %1").arg(VERSION));
		
		loadFile(textLicense, "LICENSE.txt");
		loadFile(textTranslators, "TRANSLATIONS.txt");
		loadFile(text3rdParties, "3RDPARTIES.txt");
		
#ifdef WITH_NLS
		checkFeatureNLS->setChecked(true);
#endif
#ifdef WITH_BITTORRENT
		checkFeatureBitTorrent->setChecked(true);
#endif
#ifdef WITH_SFTP
		checkFeatureSFTP->setChecked(true);
#endif
#ifdef WITH_JABBER
		checkFeatureJabber->setChecked(true);
#endif
#ifdef WITH_DOCUMENTATION
		checkFeatureDocumentation->setChecked(true);
#endif
	}
	static void loadFile(QTextEdit* edit, QString filename)
	{
		QFile file;
		QString name;
		
		name = DATA_LOCATION "/data/";
		name += filename;
		
		file.setFileName(name);
		file.open(QIODevice::ReadOnly);
		
		edit->setPlainText(QString::fromUtf8(file.readAll()));
	}
};

#endif
