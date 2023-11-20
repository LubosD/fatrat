/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#include "FileSharingSearch.h"

#include <QtDebug>

#include "MainWindow.h"
#include "RuntimeException.h"
#include "Settings.h"
#include "fatrat.h"
#include "java/JArray.h"
#include "java/JClass.h"
#include "java/JSearchPlugin.h"

QList<FileSharingSearch::SearchEngine> FileSharingSearch::m_engines;

extern QList<JObject> g_configListeners;

FileSharingSearch::FileSharingSearch(QWidget* parent) : QWidget(parent) {
  setupUi(this);
  addSearchEngines();

  connect(pushSearch, SIGNAL(clicked()), this, SLOT(searchClicked()));
  connect(pushDownload, SIGNAL(clicked()), this, SLOT(downloadClicked()));

  QHeaderView* phdr = treeResults->header();
  phdr->resizeSection(0, 350);
  phdr->resizeSection(2, 200);

  QTimer::singleShot(100, this, SLOT(setSearchFocus()));
}

void FileSharingSearch::setSearchFocus() {
  lineExpr->setFocus(Qt::OtherFocusReason);
}

void FileSharingSearch::addSearchEngines() {
  QStringList last = getSettingsValue("java/search_servers").toStringList();
  foreach (const SearchEngine& e, m_engines) {
    bool checked = false;
    QListWidgetItem* item = new QListWidgetItem(e.name, listEngines);

    if (last.contains(e.clsName)) checked = true;

    item->setData(Qt::UserRole, e.clsName);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);

    listEngines->addItem(item);
  }
}

void FileSharingSearch::globalInit() {
  try {
    JClass helper("info.dolezel.fatrat.plugins.helpers.NativeHelpers");
    JClass annotation(
        "info.dolezel.fatrat.plugins.annotations.SearchPluginInfo");

    JSearchPlugin::registerNatives();

    JArgs args;

    args << "info.dolezel.fatrat.plugins" << annotation.toVariant();

    JArray arr = helper
                     .callStatic("findAnnotatedClasses",
                                 JSignature()
                                     .addString()
                                     .add("java.lang.Class")
                                     .retA("java.lang.Class"),
                                 args)
                     .value<JArray>();
    qDebug() << "Found" << arr.size() << "annotated classes (SearchPluginInfo)";

    unsigned int classes = arr.size();
    for (unsigned int i = 0; i < classes; i++) {
      try {
        JClass obj = (jobject)arr.getObject(i);
        JObject ann = obj.getAnnotation(annotation);
        QString name = ann.call("name", JSignature().retString()).toString();
        QString clsName = obj.getClassName();
        JObject instance(obj, JSignature());

        qDebug() << "Class name:" << clsName;
        qDebug() << "Name:" << name;

        if (instance.instanceOf(
                "info.dolezel.fatrat.plugins.listeners.ConfigListener"))
          g_configListeners << instance;

        SearchEngine e = {name, clsName /*, false*/};
        m_engines << e;
      } catch (const RuntimeException& e) {
        qDebug() << e.what();
      }
    }
  } catch (const RuntimeException& e) {
    qDebug() << e.what();
  }
}

void FileSharingSearch::globalExit() { m_engines.clear(); }

void FileSharingSearch::enableControls(bool enable) {
  listEngines->setEnabled(enable);
  pushSearch->setEnabled(enable);
  pushDownload->setEnabled(enable);
  lineExpr->setEnabled(enable);
  treeResults->setEnabled(enable);
  progressBar->setEnabled(!enable);
}

void FileSharingSearch::searchClicked() {
  QString query = lineExpr->text().trimmed();

  treeResults->clear();
  progressBar->setValue(0);
  progressBar->setMaximum(0);

  if (query.isEmpty()) return;

  enableControls(false);

  int total = 0;
  QStringList classes;

  for (int i = 0; i < listEngines->count(); i++) {
    if (listEngines->item(i)->checkState() == Qt::Checked) {
      JSearchPlugin* p = 0;

      classes << listEngines->item(i)->data(Qt::UserRole).toString();

      try {
        p = new JSearchPlugin(m_engines[i].clsName);
        p->setDialog(this);

        total++;
        p->call("search", JSignature().addString(), query);

        m_remaining << p;
      } catch (...) {
        total--;
        delete p;
      }
    }
  }

  setSettingsValue("java/search_servers", classes);
  progressBar->setMaximum(total);
}

void FileSharingSearch::downloadClicked() {
  QList<QTreeWidgetItem*> sel = treeResults->selectedItems();
  QString urls;

  foreach (QTreeWidgetItem* item, sel) {
    urls += item->data(0, Qt::UserRole).toString() + "\n";
  }

  if (!urls.isEmpty()) {
    MainWindow* wnd = (MainWindow*)getMainWindow();
    wnd->addTransfer(urls, Transfer::Download);
  }
}

void FileSharingSearch::addSearchResults(QString fromClass,
                                         QList<SearchResult>& res) {
  QString name;
  for (int i = 0; i < m_engines.size(); i++) {
    if (m_engines[i].clsName == fromClass) {
      name = m_engines[i].name;
      break;
    }
  }

  foreach (const SearchResult& sr, res) {
    QTreeWidgetItem* item = new QTreeWidgetItem(treeResults);
    item->setText(0, sr.name);
    item->setText(1, formatSize(sr.fileSize));
    item->setData(0, Qt::UserRole, sr.url);
    item->setText(2, name);
    item->setText(3, sr.extraInfo);
    treeResults->addTopLevelItem(item);
  }

  searchDone(fromClass);
}

void FileSharingSearch::searchFailed(QString fromClass) {
  searchDone(fromClass);
}

void FileSharingSearch::searchDone(QString cls) {
  progressBar->setValue(progressBar->maximum() - m_remaining.size() + 1);
  qDebug() << "Progressbar: " << progressBar->value() << "/"
           << progressBar->maximum();

  for (int i = 0; i < m_remaining.size(); i++) {
    if (m_remaining[i]->getClass().getClassName() == cls) {
      m_remaining.takeAt(i)->deleteLater();
      break;
    }
  }

  checkIfFinished();
}

void FileSharingSearch::checkIfFinished() {
  if (m_remaining.isEmpty()) {
    enableControls(true);
  }
}
