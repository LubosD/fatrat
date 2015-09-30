/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#include "AboutDlg.h"
#include "config.h"
#include "fatrat.h"
#include <QFile>
#include <QHeaderView>

extern QMap<QString,PluginInfo> g_plugins;

AboutDlg::AboutDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);
	
	QStringList items;
	items << "FatRat" << tr("License") << tr("Translations") << tr("3rd parties")
			<< tr("Features") << tr("Plugins");
	
	QIcon icon(":/fatrat/miscellaneous.png");
	for(int i=0;i<items.size();i++)
	{
		QListWidgetItem* item = new QListWidgetItem(icon,items[i],listMenu);
		listMenu->addItem(item);
	}
	
	labelVersion->setText(tr("Version %1").arg(VERSION));
	
	loadFile(textLicense, "LICENSE");
	loadFile(textTranslators, "TRANSLATIONS");
	loadFile(text3rdParties, "README");
		
#ifdef WITH_NLS
	checkFeatureNLS->setChecked(true);
#endif
#ifdef WITH_BITTORRENT
	checkFeatureBitTorrent->setChecked(true);
#endif
#ifdef WITH_JABBER
	checkFeatureJabber->setChecked(true);
#endif
#ifdef WITH_DOCUMENTATION
	checkFeatureDocumentation->setChecked(true);
#endif
#ifdef WITH_WEBINTERFACE
	checkFeatureWebInterface->setChecked(true);
#endif
#ifdef WITH_JPLUGINS
	checkFeatureJavaExtensions->setChecked(true);
#endif
	
	processPlugins();
	listMenu->setCurrentRow(0);
}

void AboutDlg::processPlugins()
{
	QString text;
	for(QMap<QString,PluginInfo>::const_iterator it = g_plugins.constBegin();
		   it != g_plugins.constEnd(); it++)
	{
		text += tr("<b>%1</b><br>File name: %2<br>Author: %3<br>Web site: <a href=\"%4\">%4</a>")
				.arg(it->name).arg(it.key()).arg(it->author).arg(it->website);
		text += "<p>";
	}
	textPlugins->setHtml(text);
}

void AboutDlg::loadFile(QTextEdit* edit, QString filename)
{
	QFile file;
	QString name;
	
	name = DATA_LOCATION "/";
	name += filename;
	
	file.setFileName(name);
	file.open(QIODevice::ReadOnly);
	
	edit->setPlainText(QString::fromUtf8(file.readAll()));
}
