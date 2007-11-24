#ifndef _USERAUTHDLG_H
#define _USERAUTHDLG_H
#include <QDialog>
#include <QRegExp>
#include <QMessageBox>
#include "fatrat.h"
#include "ui_UserAuthDlg.h"

class UserAuthDlg : public QDialog, Ui_UserAuthDlg
{
Q_OBJECT
public:
	UserAuthDlg(bool bRegExpMode, QWidget* parent)
	: QDialog(parent), m_bRegExpMode(bRegExpMode)
	{
		setupUi(this);
	}
	int exec()
	{
		int r;
		
		lineRegExp->setVisible(m_bRegExpMode);
		labelRegExp->setVisible(m_bRegExpMode);
		
		lineRegExp->setText(m_auth.strRegExp);
		lineUser->setText(m_auth.strUser);
		linePassword->setText(m_auth.strPassword);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_auth.strRegExp = lineRegExp->text();
			m_auth.strUser = lineUser->text();
			m_auth.strPassword = linePassword->text();
		}
		return r;
	}
public slots:
	virtual void accept()
	{
		if(m_bRegExpMode)
		{
			QRegExp re (lineRegExp->text());
			if(!re.isValid())
			{
				QMessageBox::warning(this, "FatRat", tr("The regular expression you've entered is invalid."));
			}
			else if(!lineUser->text().isEmpty())
				QDialog::accept();
		}
		else
			QDialog::accept();
	}
private:
	bool m_bRegExpMode;
public:
	Auth m_auth;
};

#endif

