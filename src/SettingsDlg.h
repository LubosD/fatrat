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

#ifndef _SETTINGSDLG_H
#define _SETTINGSDLG_H
#include <QDialog>
#include <QList>
#include <QVector>

#include "Transfer.h"
#include "ui_SettingsDlg.h"

class SettingsDlg : public QDialog, Ui_SettingsDlg {
  Q_OBJECT
 public:
  SettingsDlg(QWidget* parent);
  ~SettingsDlg();

  void setPage(int i);
  template <typename T>
  void setPageByType() {
    for (int i = 0; i < m_children.size(); i++) {
      if (dynamic_cast<T*>(m_children[i])) {
        setPage(i);
        break;
      }
    }
  }

  virtual void accept();
  int exec();
 public slots:
  void buttonClicked(QAbstractButton* btn);

 private:
  QList<WidgetHostChild*> m_children;
};

#endif
