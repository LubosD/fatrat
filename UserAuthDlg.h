#ifndef _USERAUTHDLG_H
#define _USERAUTHDLG_H
#include <QDialog>
#include <QRegExp>
#include "fatrat.h"
#include "ui_UserAuthDlg.h"

class UserAuthDlg : public QDialog, Ui_UserAuthDlg
{
Q_OBJECT
public:
	UserAuthDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
	}
	int exec()
	{
		int r;
		
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
		QRegExp re (lineRegExp->text());
		if(re.isValid() && !lineUser->text().isEmpty())
			QDialog::accept();
	}
public:
	Auth m_auth;
};

#endif

