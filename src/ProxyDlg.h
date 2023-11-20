/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef _PROXYDLG_H
#define _PROXYDLG_H
#include <QDialog>

#include "Proxy.h"
#include "ui_ProxyDlg.h"

class ProxyDlg : public QDialog, Ui_ProxyDlg {
  Q_OBJECT
 public:
  ProxyDlg(QWidget* parent = 0) : QDialog(parent) {
    setupUi(this);
    m_data.nType = Proxy::ProxyHttp;

    comboType->addItem("HTTP");
    comboType->addItem("SOCKS 5");

    m_data.nType = Proxy::ProxyHttp;
    m_data.nPort = 80;

    lineName->setFocus(Qt::OtherFocusReason);
  }

  int exec() {
    int r;
    comboType->setCurrentIndex(m_data.nType);
    lineName->setText(m_data.strName);
    lineIP->setText(m_data.strIP);
    linePort->setText(QString::number(m_data.nPort));
    lineUser->setText(m_data.strUser);
    linePassword->setText(m_data.strPassword);

    if ((r = QDialog::exec()) == QDialog::Accepted) {
      m_data.nType = (Proxy::ProxyType)comboType->currentIndex();
      m_data.strName = lineName->text();
      m_data.strIP = lineIP->text();
      m_data.nPort = linePort->text().toUInt();
      m_data.strUser = lineUser->text();
      m_data.strPassword = linePassword->text();
    }
    return r;
  }
 public slots:
  virtual void accept() {
    if (!lineName->text().isEmpty() && !lineIP->text().isEmpty() &&
        !linePort->text().isEmpty())
      QDialog::accept();
  }

 public:
  Proxy m_data;
};

#endif
