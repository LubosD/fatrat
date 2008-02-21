#ifndef RSSFEEDDLG_H
#define RSSFEEDDLG_H
#include <QDialog>
#include "ui_RssFeedDlg.h"

class RssFeedDlg : public QDialog, Ui_RssFeedDlg
{
Q_OBJECT
public:
	RssFeedDlg(QWidget* parent)
	: QDialog(parent)
	{
		setupUi(this);
	}
	
	int exec()
	{
		int r;
		
		lineName->setText(m_strName);
		lineURL->setText(m_strURL);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_strName = lineName->text();
			m_strURL = lineURL->text();
		}
		return r;
	}
	
	void accept()
	{
		if(!lineName->text().isEmpty() && !lineURL->text().isEmpty())
			QDialog::accept();
	}
	
	QString m_strName, m_strURL;
};

#endif
