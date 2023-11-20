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

#include "SettingsDlg.h"

#include <QSettings>

#include "MainWindow.h"
#include "Settings.h"
#include "config.h"
#include "fatrat.h"

extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QVector<SettingsItem> g_settingsPages;

SettingsDlg::SettingsDlg(QWidget* parent) : QDialog(parent) {
  setupUi(this);

  for (int i = 0; i < g_settingsPages.size(); i++) {
    QWidget* w = new QWidget(stackedWidget);
    m_children << g_settingsPages[i].lpfnCreate(w, this);
    stackedWidget->addWidget(w);
    listWidget->addItem(new QListWidgetItem(
        g_settingsPages[i].icon, g_settingsPages[i].title, listWidget));
  }

  listWidget->setCurrentRow(0);

  connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this,
          SLOT(buttonClicked(QAbstractButton*)));
}

SettingsDlg::~SettingsDlg() { qDeleteAll(m_children); }

void SettingsDlg::setPage(int i) {
  listWidget->setCurrentRow(i);
  stackedWidget->setCurrentIndex(i);
}

void SettingsDlg::accept() {
  bool accepted = true, acc;

  foreach (WidgetHostChild* w, m_children) {
    acc = w->accept();
    accepted = accepted && acc;

    if (!accepted) break;
  }

  if (accepted) {
    foreach (WidgetHostChild* w, m_children) w->accepted();

    QDialog::accept();

    g_settings->sync();
    static_cast<MainWindow*>(getMainWindow())->reconfigure();
  }
}

int SettingsDlg::exec() {
  foreach (WidgetHostChild* w, m_children) w->load();

  return QDialog::exec();
}

void SettingsDlg::buttonClicked(QAbstractButton* btn) {
  if (buttonBox->buttonRole(btn) == QDialogButtonBox::ApplyRole) {
    foreach (WidgetHostChild* w, m_children) {
      bool acc = w->accept();

      if (!acc) return;
    }

    foreach (WidgetHostChild* w, m_children) w->accepted();
    foreach (WidgetHostChild* w, m_children) w->load();

    static_cast<MainWindow*>(getMainWindow())->reconfigure();
  }
}
