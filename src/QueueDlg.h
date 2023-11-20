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

#ifndef _QUEUEDLG_H
#define _QUEUEDLG_H
#include <QDialog>

#include "ui_QueueDlg.h"

class QueueDlg : public QDialog, Ui_QueueDlg {
  Q_OBJECT
 public:
  QueueDlg(QWidget* parent = 0);
  int exec();
 public slots:
  virtual void accept();
  void limitToggled(bool checked);
  void browse();
  void browseMove();

 public:
  QString m_strName, m_strDefaultDirectory, m_strMoveDirectory;
  int m_nDownLimit, m_nUpLimit, m_nDownTransfersLimit, m_nUpTransfersLimit;
  bool m_bUpAsDown, m_bLimit;
};

#endif
