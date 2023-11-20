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

#include "SettingsGeneralForm.h"

#include <QDir>
#include <QMessageBox>
#include <QSettings>

#include "MainWindow.h"
#include "Settings.h"
#include "fatrat.h"

extern QSettings* g_settings;

SettingsGeneralForm::SettingsGeneralForm(QWidget* me, QObject* parent)
    : QObject(parent) {
  setupUi(me);

  comboDoubleClick->addItems(QStringList() << tr("switches to transfer details")
                                           << tr("switches to the graph")
                                           << tr("opens the file")
                                           << tr("opens the parent directory"));

  comboCloseCurrent->addItems(QStringList()
                              << tr("switch to the next tab")
                              << tr("switch to the previous active tab"));

  comboLinkSeparator->addItems(QStringList() << tr("a newline")
                                             << tr("whitespace characters"));
  comboFileExec->addItems(QStringList() << "xdg-open"
                                        << "kfmclient exec"
                                        << "gnome-open");
  comboGraphStyle->addItems(QStringList()
                            << tr("Filled graph") << tr("Line graph"));

  // connect(labelManageFavs, SIGNAL(linkActivated(const QString&)), this,
  // SLOT(manageFavs()));
}

void SettingsGeneralForm::load() {
  comboFileExec->setEditText(getSettingsValue("fileexec").toString());

  checkTrayIcon->setChecked(getSettingsValue("trayicon").toBool());
  checkHideMinimize->setChecked(getSettingsValue("hideminimize").toBool());
  checkHideClose->setChecked(getSettingsValue("hideclose").toBool());
  checkHideUnfocused->setChecked(
      getSettingsValue("gui/hideunfocused").toBool());

  spinGraphMinutes->setValue(getSettingsValue("graphminutes").toInt());
  comboGraphStyle->setCurrentIndex(getSettingsValue("graph_style").toInt());

  comboDoubleClick->setCurrentIndex(
      getSettingsValue("transfer_dblclk").toInt());
  comboCloseCurrent->setCurrentIndex(getSettingsValue("tab_onclose").toInt());
  comboLinkSeparator->setCurrentIndex(
      getSettingsValue("link_separator").toInt());

  spinRefreshGUI->setValue(getSettingsValue("gui_refresh").toInt() / 1000);
  checkCSS->setChecked(getSettingsValue("css").toBool());
  checkSystemTheme->setChecked(getSettingsValue("gui/systemtheme").toBool());
}

void SettingsGeneralForm::accepted() {
  setSettingsValue("fileexec", comboFileExec->currentText());

  setSettingsValue("trayicon", checkTrayIcon->isChecked());
  setSettingsValue("hideminimize", checkHideMinimize->isChecked());
  setSettingsValue("hideclose", checkHideClose->isChecked());
  setSettingsValue("gui/hideunfocused", checkHideUnfocused->isChecked());

  setSettingsValue("graphminutes", spinGraphMinutes->value());
  setSettingsValue("graph_style", comboGraphStyle->currentIndex());
  setSettingsValue("transfer_dblclk", comboDoubleClick->currentIndex());
  setSettingsValue("tab_onclose", comboCloseCurrent->currentIndex());
  setSettingsValue("link_separator", comboLinkSeparator->currentIndex());

  setSettingsValue("gui_refresh", spinRefreshGUI->value() * 1000);
  setSettingsValue("css", checkCSS->isChecked());
  setSettingsValue("gui/systemtheme", checkSystemTheme->isChecked());

  applySettings();
}

void SettingsGeneralForm::applySettings() {
  if (programHasGUI()) ((MainWindow*)getMainWindow())->applySettings();
}

void SettingsGeneralForm::manageFavs() {}
