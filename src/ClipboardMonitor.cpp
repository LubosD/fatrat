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

#include "ClipboardMonitor.h"

#include <QApplication>

#include "MainWindow.h"
#include "Settings.h"
#include "fatrat.h"

ClipboardMonitor* ClipboardMonitor::m_instance = 0;

ClipboardMonitor::ClipboardMonitor() {
  m_instance = this;
  m_clipboard = QApplication::clipboard();
  connect(m_clipboard, SIGNAL(changed(QClipboard::Mode)), this,
          SLOT(dataChanged(QClipboard::Mode)));
  reloadSettings();
}

ClipboardMonitor::~ClipboardMonitor() { m_instance = 0; }

void ClipboardMonitor::reloadSettings() {
  m_bEnabledGlobal = getSettingsValue("clipboard/monitorglobal").toBool();
  m_bEnabledSelection = getSettingsValue("clipboard/monitorselection").toBool();
  QStringList r = getSettingsValue("clipboard/regexps").toStringList();
  QList<QRegExp> nr;

  foreach (const QString& s, r) {
    nr << QRegExp(s);
  }
  m_regexps = nr;
}

void ClipboardMonitor::dataChanged(QClipboard::Mode mode) {
  if ((!m_bEnabledGlobal && mode == QClipboard::Clipboard) ||
      (!m_bEnabledSelection && mode == QClipboard::Selection))
    return;

  QString text = m_clipboard->text(mode);
  if (text.isEmpty()) return;

  QStringList links;

  foreach (const QRegExp& re, m_regexps) {
    int pos = 0, start = links.size();

    while ((pos = re.indexIn(text, pos)) != -1) {
      links << re.cap(0);
      pos += re.cap(0).length();
    }

    for (int i = start; i < links.size(); i++) text.remove(links[i]);
  }

  if (!links.isEmpty()) {
    links.removeDuplicates();
    MainWindow* w = static_cast<MainWindow*>(getMainWindow());
    if (w) w->addTransfer(links.join("\n"));
  }
}
