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
		
		labelVersion->setText(tr("<b>Version %1</b>").arg(VERSION));
		
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
