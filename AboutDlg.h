#ifndef ABOUTDLG_H
#define ABOUTDLG_H

#include <QDialog>
#include "ui_AboutDlg.h"

class AboutDlg : public QDialog, Ui_AboutDlg
{
Q_OBJECT
public:
	AboutDlg(QWidget* parent);
	void processPlugins();
	static void loadFile(QTextEdit* edit, QString filename);
};

#endif
