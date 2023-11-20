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

#include "TorrentSearch.h"

#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QHeaderView>
#include <QMap>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QtAlgorithms>
#include <QtDebug>

#include "MainWindow.h"
#include "Proxy.h"
#include "RuntimeException.h"
#include "Settings.h"
#include "TorrentWebView.h"
#include "config.h"
#include "fatrat.h"

extern QSettings* g_settings;

static const char* BTSEARCH_DIR = "/data/btsearch/";

TorrentSearch::TorrentSearch() : m_bSearching(false) {
  setupUi(this);
  updateUi();

  connect(pushSearch, SIGNAL(clicked()), this, SLOT(search()));
  connect(treeResults, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this,
          SLOT(download()));
  connect(pushDownload, SIGNAL(clicked()), this, SLOT(download()));
  connect(treeResults, SIGNAL(customContextMenuRequested(const QPoint&)), this,
          SLOT(resultsContext(const QPoint&)));

  loadEngines();

  g_settings->beginGroup("torrent/torrent_search");

  foreach (Engine e, m_engines) {
    QListWidgetItem* item = new QListWidgetItem(e.name, listEngines);
    bool checked;

    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);

    checked = g_settings->value(e.name, false).toBool();

    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    listEngines->addItem(item);
  }

  g_settings->endGroup();

  QStringList hdr = QStringList() << tr("Name") << tr("Size") << tr("Seeders")
                                  << tr("Leechers") << tr("Source");
  treeResults->setHeaderLabels(hdr);
  treeResults->sortItems(3, Qt::DescendingOrder);

  QHeaderView* phdr = treeResults->header();
  phdr->resizeSection(0, 350);

  QTimer::singleShot(100, this, SLOT(setSearchFocus()));
  m_network = new QNetworkAccessManager(this);
  m_network->setProxy(Proxy::getProxy(QUuid::fromString(
      g_settings->value("torrent/proxy_tracker").toString())));
  connect(m_network, SIGNAL(finished(QNetworkReply*)), this,
          SLOT(searchDone(QNetworkReply*)));
}

TorrentSearch::~TorrentSearch() {
  g_settings->beginGroup("torrent/torrent_search");

  for (int i = 0; i < listEngines->count(); i++) {
    QListWidgetItem* item = listEngines->item(i);
    g_settings->setValue(item->text(), item->checkState() == Qt::Checked);
  }

  g_settings->endGroup();
}

void TorrentSearch::setSearchFocus() {
  lineExpr->setFocus(Qt::OtherFocusReason);
}

void TorrentSearch::loadEngines() {
  QStringList files = listDataDir(BTSEARCH_DIR);
  foreach (QString file, files) {
    if (file.endsWith(".xml")) loadEngines(file);
  }
}

void TorrentSearch::loadEngines(QString path) {
  QDomDocument doc;
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly) || !doc.setContent(&file)) {
    qDebug() << "Failed to open/read " << path;
    return;
  }

  QDomElement n = doc.documentElement().firstChildElement("engine");
  while (!n.isNull()) {
    Engine e;

    e.reply = 0;

    e.name = n.attribute("name");
    e.id = n.attribute("id");

    QDomElement sn = n.firstChildElement("param");
    while (!sn.isNull()) {
      QString contents = sn.firstChild().toText().data();
      QString type = sn.attribute("name");

      if (type == "query")
        e.query = contents;
      else if (type == "postdata")
        e.postData = contents;
      else if (type == "beginning")
        e.beginning = contents;
      else if (type == "splitter")
        e.splitter = contents;
      else if (type == "ending")
        e.ending = contents;

      sn = sn.nextSiblingElement("param");
    }

    sn = n.firstChildElement("regexp");
    while (!sn.isNull()) {
      RegExpParam param;
      QString type = sn.attribute("name");

      param.regexp = sn.text();
      param.field = sn.attribute("field", "0").toInt();
      param.match = sn.attribute("match", "0").toInt();

      e.regexps[type] = param;

      sn = sn.nextSiblingElement("regexp");
    }

    sn = n.firstChildElement("format");
    while (!sn.isNull()) {
      QString format = sn.text();
      QString type = sn.attribute("name");

      e.formats[type] = format;

      sn = sn.nextSiblingElement("format");
    }

    m_engines << e;

    n = n.nextSiblingElement("engine");
  }
  std::sort(m_engines.begin(), m_engines.end());
}

void TorrentSearch::search() {
  if (m_bSearching) {
    for (int i = 0; i < m_engines.size(); i++) {
      if (m_engines[i].reply) {
        m_engines[i].reply->abort();
        m_engines[i].reply->deleteLater();
        m_engines[i].reply = 0;
      }
    }

    m_bSearching = false;
    updateUi();
  } else {
    bool bSel = false;
    QString expr = lineExpr->text().trimmed();

    if (expr.isEmpty()) return;

    emit changeTabTitle(tr("Torrent search") + ": " + expr);

    m_nActiveTotal = m_nActiveDone = 0;

    for (int i = 0; i < listEngines->count(); i++) {
      if (listEngines->item(i)->checkState() == Qt::Checked) {
        QUrl url;
        const QString& query = m_engines[i].query;

        if (query.contains("%1"))
          url = query.arg(expr);
        else
          url = query;

        QString path = url.path();

        path.replace(' ', '+');

        if (!m_engines[i].postData.isEmpty()) {
          QString postQuery;
          QByteArray postEnc;

          postQuery = m_engines[i].postData.arg(expr);
          postQuery.replace(' ', '+');

          postEnc = postQuery.toUtf8();

          m_engines[i].reply = m_network->post(QNetworkRequest(url), postEnc);
        } else
          m_engines[i].reply = m_network->get(QNetworkRequest(url));

        bSel = true;
        m_nActiveTotal++;
      }
    }

    if (bSel) {
      progressBar->setValue(0);
      progressBar->setMaximum(m_nActiveTotal);

      treeResults->clear();

      m_bSearching = true;
      updateUi();
    } else {
      QMessageBox::warning(getMainWindow(), "FatRat",
                           tr("Please enable at least one search engine."));
    }
  }
}

void TorrentSearch::parseResults(Engine* e, const QByteArray& data) {
  pushDownload->setEnabled(true);
  QList<QByteArray> results;

  try {
    int end, start;

    start = data.indexOf(e->beginning.toUtf8());
    if (start < 0)
      throw RuntimeException(
          "Error parsing search engine's response - 'start'");

    end = data.indexOf(e->ending.toUtf8(), start);
    if (end < 0)
      throw RuntimeException("Error parsing search engine's response - 'end'");

    start += e->beginning.size();
    results = splitArray(data.mid(start, end - start), e->splitter);

    qDebug() << "Results:" << results.size();
  } catch (const RuntimeException& e) {
    qDebug() << e.what();
    return;
  }

  foreach (QByteArray ar, results) {
    try {
      QMap<QString, QString> map;

      for (QMap<QString, RegExpParam>::iterator it = e->regexps.begin();
           it != e->regexps.end(); it++) {
        QRegularExpression re(it.value().regexp);
        int pos = 0;

        QRegularExpressionMatch match;
        for (int k = 0; k < it.value().match + 1; k++) {
          match = re.match(ar, pos);

          if (!match.hasMatch())
            throw RuntimeException(QString("Failed to match \"%1\" in \"%2\"")
                                       .arg(it.key())
                                       .arg(QString(ar)));
          else
            pos = match.capturedEnd(0);  // Move to the end of the match
        }

        QString text;
        if (e->formats.contains(it.key())) {
          text = e->formats[it.key()];
          for (int i = 0; i < re.captureCount(); i++)
            text = text.arg(match.captured(i + 1));
        } else {
          text = match.captured(it.value().field + 1);
        }

        QTextDocument doc;  // FIXME: ineffective?
        doc.setHtml(text);
        map[it.key()] = doc.toPlainText().trimmed().replace('\n', ' ');
      }

      SearchTreeWidgetItem* item = new SearchTreeWidgetItem(treeResults);
      item->setText(0, map["name"]);
      item->setText(1, map["size"]);
      item->parseSize(map["size"]);
      item->setText(2, map["seeders"]);
      item->setText(3, map["leechers"]);
      item->setText(4, e->name);

      item->m_strLink = completeUrl(map["link"], e->query);
      item->m_strInfo = completeUrl(map["info"], e->query);

      treeResults->addTopLevelItem(item);
    } catch (const RuntimeException& e) {
      qDebug() << e.what();
    }
  }

  progressBar->setValue(++m_nActiveDone);
}

QString TorrentSearch::completeUrl(QString surl, QString complete) {
  if (surl.startsWith("http://") || surl.startsWith("magnet:")) return surl;

  QUrl url(complete);

  if (surl[0] != '/') {
    QString path = url.path();
    int ix = path.lastIndexOf('/');
    if (ix != -1) {
      path.resize(ix + 1);
      surl.prepend(path);
    }
  }

  if (url.port(80) != 80)  // some sites such as Mininova hate ':80'
    return QString("%1://%2:%3%4")
        .arg(url.scheme())
        .arg(url.host())
        .arg(url.port(80))
        .arg(surl);
  else
    return QString("%1://%2%3").arg(url.scheme()).arg(url.host()).arg(surl);
}

void TorrentSearch::searchDone(QNetworkReply* reply) {
  reply->deleteLater();

  Engine* e = 0;

  for (int i = 0; i < m_engines.size(); i++) {
    if (m_engines[i].reply == reply) {
      e = &m_engines[i];
      break;
    }
  }

  if (!e) return;

  e->reply = 0;

  if (reply->error() == QNetworkReply::NoError)
    parseResults(e, reply->readAll());

  m_bSearching = false;
  for (int i = 0; i < m_engines.size(); i++) {
    if (m_engines[i].reply != 0) {
      m_bSearching = true;
      break;
    }
  }

  treeResults->setSortingEnabled(true);
  updateUi();
}

void TorrentSearch::updateUi() {
  pushSearch->setText(m_bSearching ? tr("Stop searching") : tr("Search"));
  lineExpr->setDisabled(m_bSearching);
  listEngines->setDisabled(m_bSearching);
  progressBar->setVisible(m_bSearching);
}

QList<QByteArray> TorrentSearch::splitArray(const QByteArray& src,
                                            QString sep) {
  int n, split = 0;
  QList<QByteArray> out;

  while ((n = src.indexOf(sep.toUtf8(), split)) != -1) {
    out << src.mid(split, n - split);
    split = n + sep.size();
  }

  out << src.mid(split);

  return out;
}

void TorrentSearch::download() {
  QModelIndexList list = treeResults->selectionModel()->selectedRows();
  QList<int> result;
  QString links;

  foreach (QModelIndex in, list) {
    int row = in.row();
    SearchTreeWidgetItem* it =
        (SearchTreeWidgetItem*)treeResults->topLevelItem(row);
    links += it->m_strLink;
    links += '\n';
  }

  if (!links.isEmpty()) {
    MainWindow* wnd = (MainWindow*)getMainWindow();
    wnd->addTransfer(links, Transfer::Download, "TorrentDownload");
  }
}

void TorrentSearch::resultsContext(const QPoint&) {
  QMenu menu(treeResults);
  QAction* act;
  QList<QTreeWidgetItem*> items = treeResults->selectedItems();

  if (items.isEmpty()) return;

  act = menu.addAction(tr("Download"));
  connect(act, SIGNAL(triggered()), this, SLOT(download()));

  act = menu.addAction(tr("Open details page"));

  act->setDisabled(
      items.size() > 1 ||
      static_cast<SearchTreeWidgetItem*>(items[0])->m_strInfo.isEmpty());
  connect(act, SIGNAL(triggered()), this, SLOT(openDetails()));

  menu.exec(QCursor::pos());
}

void TorrentSearch::openDetails() {
  QList<QTreeWidgetItem*> items = treeResults->selectedItems();
  QString url;

  if (items.size() != 1) return;

  url = static_cast<SearchTreeWidgetItem*>(items[0])->m_strInfo;

  if (getSettingsValue("torrent/details_mode") == 0) {
    MainWindow* wnd = static_cast<MainWindow*>(getMainWindow());
    TorrentWebView* w = new TorrentWebView(wnd->tabMain);
    connect(w, SIGNAL(changeTabTitle(QString)), wnd->tabMain,
            SLOT(changeTabTitle(QString)));
    wnd->tabMain->setCurrentIndex(
        wnd->tabMain->addTab(w, QIcon(), tr("Torrent details")));

    qDebug() << "Navigating to" << url;

    w->browser->load(url);
  } else {
    QDesktopServices::openUrl(url);
  }
}

/////////////////////////////////

bool SearchTreeWidgetItem::operator<(const QTreeWidgetItem& other) const {
  int column = treeWidget()->sortColumn();
  if (column == 1)  // size
  {
    const SearchTreeWidgetItem* item = (SearchTreeWidgetItem*)&other;
    return m_nSize < item->m_nSize;
  } else if (column == 0)  // name
    return text(column).toLower() < other.text(column).toLower();
  else if (column == 2 || column == 3)  // seeders & leechers
    return text(column).toInt() < other.text(column).toInt();
  else
    return QTreeWidgetItem::operator<(other);
}

void SearchTreeWidgetItem::parseSize(QString in) {
  int split = -1;

  if (in.isEmpty()) {
    setText(1, "?");
    return;
  }

  for (int i = 0; i < in.size(); i++) {
    if (!in[i].isDigit() && in[i] != '.') {
      split = i;
      break;
    }
  }

  if (split < 0) {
    // qDebug() << "Unable to parse size:" << in;
    split = in.size();
    in += "mb";
  }
  // else
  {
    QString units;
    double size = in.left(split).toDouble();

    units = in.mid(split).trimmed();

    if (!units.compare("k", Qt::CaseInsensitive) ||
        !units.compare("kb", Qt::CaseInsensitive) ||
        !units.compare("kib", Qt::CaseInsensitive)) {
      size *= 1024;
    } else if (!units.compare("m", Qt::CaseInsensitive) ||
               !units.compare("mb", Qt::CaseInsensitive) ||
               !units.compare("mib", Qt::CaseInsensitive)) {
      size *= 1024LL * 1024LL;
    } else if (!units.compare("g", Qt::CaseInsensitive) ||
               !units.compare("gb", Qt::CaseInsensitive) ||
               !units.compare("gib", Qt::CaseInsensitive)) {
      size *= 1024LL * 1024LL * 1024LL;
    }

    m_nSize = qint64(size);
    setText(1, formatSize(m_nSize));
  }
}
