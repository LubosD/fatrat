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

#include "HttpMirrorsDlg.h"

#include <QProcess>
#include <QRegularExpression>
#include <QUrl>
#include <QtDebug>
#include <climits>

HttpMirrorsDlg::HttpMirrorsDlg(QWidget* parent)
    : QDialog(parent), m_probeThread(0) {
  setupUi(this);
  treeMirrors->setColumnWidth(0, 250);
  treeMirrors->sortItems(2, Qt::AscendingOrder);
}

HttpMirrorsDlg::~HttpMirrorsDlg() {
  if (m_probeThread) {
    m_probeThread->stop();
    if (!m_probeThread->wait(1500)) m_probeThread->terminate();
  }
}

void HttpMirrorsDlg::load(const QMap<QString, QStringList>& mirrors,
                          QSet<QString> compatible) {
  QMap<QString, QTreeWidgetItem*> toProbe;
  foreach (QString grp, compatible) {
    QTreeWidgetItem* item = new QTreeWidgetItem(treeMirrors);
    item->setText(0, grp);
    treeMirrors->addTopLevelItem(item);
    item->setExpanded(true);

    const QStringList& servers = mirrors[grp];
    foreach (QString server, servers) {
      QUrl url = server;
      QString host = url.host();

      QTreeWidgetItem* sitem = new CSTreeWidgetItem(item);
      sitem->setText(0, host);
      sitem->setData(0, Qt::UserRole, url.toString());
      sitem->setData(1, Qt::UserRole, INT_MAX);
      sitem->setData(2, Qt::UserRole, INT_MAX);
      sitem->setFlags(sitem->flags() | Qt::ItemIsUserCheckable);
      sitem->setCheckState(0, Qt::Unchecked);
      toProbe[host] = sitem;
    }
  }

  m_probeThread = new ProbeThread(toProbe, this);
  m_probeThread->start();
}

QMap<QString, QStringList> HttpMirrorsDlg::pickedUrls() const {
  QMap<QString, QStringList> rv;
  for (int i = 0; i < treeMirrors->topLevelItemCount(); i++) {
    QTreeWidgetItem* root = treeMirrors->topLevelItem(i);
    for (int j = 0; j < root->childCount(); j++) {
      QTreeWidgetItem* item = root->child(j);
      if (item->checkState(0) == Qt::Checked)
        rv[root->text(0)] << item->data(0, Qt::UserRole).toString();
    }
  }
  return rv;
}

HttpMirrorsDlg::ProbeThread::ProbeThread(
    QMap<QString, QTreeWidgetItem*>& servers, QObject* parent)
    : QThread(parent), m_servers(servers), m_bStop(false) {}

HttpMirrorsDlg::ProbeThread::~ProbeThread() {}

void HttpMirrorsDlg::ProbeThread::run() {
  QRegularExpression reTime("time=([0-9\\.]+) ms");
  QRegularExpression reTtl("ttl=(\\d+)");

  for (QMap<QString, QTreeWidgetItem*>::iterator it = m_servers.begin();
       it != m_servers.end(); it++) {
    QProcess prc;
    int ttl = -1, ms = -1;

    if (m_bStop) break;

    qDebug() << "ping" << it.key();
    prc.start("ping",
              QStringList() << "-c1"
                            << "-w1" << it.key(),
              QIODevice::ReadOnly);
    prc.waitForFinished(2000);

    while (!prc.atEnd()) {
      QString line = prc.readLine();
      QRegularExpressionMatch matchTime = reTime.match(line);
      QRegularExpressionMatch matchTtl = reTtl.match(line);

      if (matchTime.hasMatch()) ms = matchTime.captured(1).toDouble();
      if (matchTtl.hasMatch()) ttl = matchTtl.captured(1).toInt();
    }

    QTreeWidgetItem* item = it.value();
    if (ms == -1) {
      item->setText(1, "?");
      item->setText(2, "?");
      item->setData(1, Qt::UserRole, INT_MAX - 1);
      item->setData(2, Qt::UserRole, INT_MAX - 1);
    } else {
      qDebug() << it.key() << "->" << ms;
      item->setText(1, QString("%1 ms").arg(ms));
      item->setData(1, Qt::UserRole, ms);

      if (ttl == -1)
        item->setText(2, "?");
      else {
        // guess source TTL
        int sttl;
        if (ttl > 128)
          sttl = 255;
        else if (ttl > 64)
          sttl = 128;
        else
          sttl = 64;
        item->setText(2, QString::number(sttl - ttl));
        item->setData(2, Qt::UserRole, sttl - ttl);
      }
    }
  }
}

HttpMirrorsDlg::CSTreeWidgetItem::CSTreeWidgetItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent) {}

bool HttpMirrorsDlg::CSTreeWidgetItem::operator<(
    const QTreeWidgetItem& other) const {
  int sc = treeWidget()->sortColumn();
  if (sc == 0) return QTreeWidgetItem::operator<(other);
  int data = this->data(sc, Qt::UserRole).toInt();
  int odata = other.data(sc, Qt::UserRole).toInt();
  return data < odata;
}
