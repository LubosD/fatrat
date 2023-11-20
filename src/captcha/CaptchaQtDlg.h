/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef CAPTCHAQTDLG_H
#define CAPTCHAQTDLG_H
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include "ui_CaptchaQtDlg.h"

class CaptchaQtDlg : public QDialog, Ui_CaptchaQtDlg {
  Q_OBJECT
 public:
  CaptchaQtDlg(QWidget* parent = 0);
  void load(QString url);
  virtual void accept();
  virtual void reject();
 signals:
  void captchaEntered(QString text = QString());
 private slots:
  void secondElapsed();
  void imageLoaded(QNetworkReply* reply);
  void textChanged();

 private:
  void updateInfo();

 private:
  QString m_strUrl;
  QTimer m_timer;
  int m_nSecondsLeft;
  QNetworkAccessManager m_network;
};

#endif  // CAPTCHAQTDLG_H
