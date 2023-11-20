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

#ifndef HASHDLG_H
#define HASHDLG_H
#include <QCryptographicHash>
#include <QDialog>
#include <QFile>
#include <QThread>

#include "ui_HashDlg.h"

class HashThread : public QThread {
  Q_OBJECT
 public:
  HashThread(QFile* file);
  void setAlgorithm(QCryptographicHash::Algorithm alg) { m_alg = alg; }
  void run();
  void stop() { m_bStop = true; }
 signals:
  void progress(int pcts);
  void finished(QByteArray result);

 private:
  QCryptographicHash::Algorithm m_alg;
  QFile* m_file;
  bool m_bStop;
};

class HashDlg : public QDialog, Ui_HashDlg {
  Q_OBJECT
 public:
  HashDlg(QWidget* parent = 0, QString file = QString());
  ~HashDlg();

  static QWidget* create() { return new HashDlg; }
 private slots:
  void browse();
  void compute();
  void finished(QByteArray result);

 private:
  QFile m_file;
  HashThread m_thread;
};

#endif
