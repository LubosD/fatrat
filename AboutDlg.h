#ifndef ABOUTDLG_H
#define ABOUTDLG_H
#include <QDialog>
#include <QFile>
#include "fatrat.h"
#include "ui_AboutDlg.h"

class AboutDlg : public QDialog, Ui_AboutDlg
{
Q_OBJECT
public:
	AboutDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		
		listMenu->addItem("FatRat");
		listMenu->addItem(tr("License"));
		listMenu->addItem(tr("Translations"));
		listMenu->addItem(tr("3rd parties"));
		
		labelVersion->setText(tr("<b>Version %1</b>").arg(VERSION));
		
		loadFile(textLicense, "LICENSE.txt");
		loadFile(textTranslators, "TRANSLATORS.txt");
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
		
		edit->setPlainText(file.readAll());
	}
};

#endif
