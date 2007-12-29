#ifndef _PROXYDLG_H
#define _PROXYDLG_H
#include <QDialog>
#include "ui_ProxyDlg.h"
#include "fatrat.h"

class ProxyDlg : public QDialog, Ui_ProxyDlg
{
Q_OBJECT
public:
	ProxyDlg(QWidget* parent = 0) :QDialog(parent)
	{
		setupUi(this);
		m_data.nType = Proxy::ProxyHttp;
		
		comboType->addItem("HTTP");
		comboType->addItem("SOCKS 5");
		
		m_data.nType = Proxy::ProxyHttp;
		m_data.nPort = 80;
		
		lineName->setFocus(Qt::OtherFocusReason);
	}
	
	int exec()
	{
		int r;
		comboType->setCurrentIndex(m_data.nType);
		lineName->setText(m_data.strName);
		lineIP->setText(m_data.strIP);
		linePort->setText(QString::number(m_data.nPort));
		lineUser->setText(m_data.strUser);
		linePassword->setText(m_data.strPassword);
		
		if((r = QDialog::exec()) == QDialog::Accepted)
		{
			m_data.nType = (Proxy::ProxyType) comboType->currentIndex();
			m_data.strName = lineName->text();
			m_data.strIP = lineIP->text();
			m_data.nPort = linePort->text().toUInt();
			m_data.strUser = lineUser->text();
			m_data.strPassword = linePassword->text();
		}
		return r;
	}
public slots:
	virtual void accept()
	{
		if(!lineName->text().isEmpty() && !lineIP->text().isEmpty() && !linePort->text().isEmpty())
			QDialog::accept();
	}
public:
	Proxy m_data; // fatrat.h
};

#endif
