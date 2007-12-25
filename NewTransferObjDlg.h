#ifndef NEWTRANSFEROBJDLG_H
#define NEWTRANSFEROBJDLG_H
#include <QDialog>
#include "ui_NewTransferObjDlg.h"
#include <QFileDialog>

class NewTransferObjDlg : public QDialog, Ui_NewTransferObjDlg
{
Q_OBJECT
public:
	NewTransferObjDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		
		connect(lineURL, SIGNAL(textChanged(const QString&)), this, SLOT(switchToURL()));
		connect(lineFile, SIGNAL(textChanged(const QString&)), this, SLOT(switchToFile()));
		connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browseFile()));
	}
public slots:
	void switchToURL()
	{
		radioAddUrl->setChecked(true);
	}
	void switchToFile()
	{
		radioAddFile->setChecked(true);
	}
	void browseFile()
	{
		QString file = QFileDialog::getOpenFileName(this, "FatRat");
		if(!file.isEmpty())
			lineFile->setText(file);
	}
public:
	QString getResult()
	{
		if(radioAddUrl->isChecked())
			return lineURL->text();
		else
			return lineFile->text();
	}
};

#endif

