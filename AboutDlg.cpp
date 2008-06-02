#include "AboutDlg.h"
#include "config.h"
#include "fatrat.h"
#include <QFile>
#include <QHeaderView>

extern QMap<QString,PluginInfo> g_plugins;

AboutDlg::AboutDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);
	
	QTableWidgetItem* first;
	QStringList items;
	items << "FatRat" << tr("License") << tr("Translations") << tr("3rd parties")
			<< tr("Features") << tr("Plugins");
	
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
	
	processPlugins();
}

void AboutDlg::processPlugins()
{
	QString text;
	for(QMap<QString,PluginInfo>::const_iterator it = g_plugins.constBegin();
		   it != g_plugins.constEnd(); it++)
	{
		text += QString("<b>%1</b><br>File name: %2<br>Author: %3<br>Web site: <a href=\"%4\">%4</a>")
				.arg(it->name).arg(it.key()).arg(it->author).arg(it->website);
		text += "<p>";
	}
	textPlugins->setHtml(text);
}

void AboutDlg::loadFile(QTextEdit* edit, QString filename)
{
	QFile file;
	QString name;
	
	name = DATA_LOCATION "/data/";
	name += filename;
	
	file.setFileName(name);
	file.open(QIODevice::ReadOnly);
	
	edit->setPlainText(QString::fromUtf8(file.readAll()));
}
