#ifndef ABOUTDLG_H
#define ABOUTDLG_H
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
		
		QTableWidgetItem* first = new QTableWidgetItem(QIcon(":/fatrat/fatrat.png"), "FatRat");
		first->setTextAlignment(Qt::AlignCenter);
		tableMenu->setItem(0, 0, first);
		
		QStringList items;
		items << tr("License") << tr("Translations") << tr("3rd parties");
		
		for(int i=0;i<items.size();i++)
		{
			QTableWidgetItem* item = new QTableWidgetItem(items[i]);
			item->setTextAlignment(Qt::AlignCenter);
			tableMenu->setItem(i+1, 0, item);
		}
		
		tableMenu->setCurrentItem(first);
		tableMenu->horizontalHeader()->hide();
		tableMenu->verticalHeader()->hide();
		tableMenu->setColumnWidth(0, 116);
		
		labelVersion->setText(tr("<b>Version %1</b>").arg(VERSION));
		
		loadFile(textLicense, "LICENSE.txt");
		loadFile(textTranslators, "TRANSLATIONS.txt");
		loadFile(text3rdParties, "3RDPARTIES.txt");
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
