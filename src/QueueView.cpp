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

#include "QueueView.h"

#include <QFile>
#include <QtDebug>

#include "MainWindow.h"
#include "Queue.h"
#include "Settings.h"
#include "fatrat.h"

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;

QueueView::QueueView(QWidget* parent) : QTreeWidget(parent), m_status(0) {
  if (getSettingsValue("css").toBool()) {
    QFile file;
    if (openDataFile(&file, "/data/css/queueview.css"))
      setStyleSheet(file.readAll());
  }
}

bool QueueView::dropMimeData(QTreeWidgetItem* parent, int index,
                             const QMimeData* data, Qt::DropAction action) {
  if (action == Qt::IgnoreAction) return true;

  int queueTo = parent->treeWidget()->indexOfTopLevelItem(parent);

  if (data->hasFormat("application/x-fatrat-transfer")) {
    g_queuesLock.lockForRead();

    QByteArray encodedData = data->data("application/x-fatrat-transfer");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    int queueFrom;
    QList<int> transfers;
    QList<Transfer*> objects;
    Queue* q;

    stream >> queueFrom >> transfers;

    if (queueFrom != queueTo && queueTo < g_queues.size()) {
      q = g_queues[queueFrom];
      q->lockW();

      for (int i = 0; i < transfers.size(); i++)
        objects << q->take(transfers[i] - i, true);

      q->unlock();

      q = g_queues[queueTo];
      q->add(objects);
    }

    g_queuesLock.unlock();
  } else if (data->hasFormat("text/plain")) {
    MainWindow* wnd = (MainWindow*)getMainWindow();
    wnd->addTransfer(data->data("text/plain"), Transfer::ModeInvalid, QString(),
                     queueTo);
  } else
    return false;
  return true;
}

QStringList QueueView::mimeTypes() const {
  return QStringList() << "application/x-fatrat-transfer"
                       << "text/plain";
}

void QueueView::mouseMoveEvent(QMouseEvent* event) {
  if (!getSettingsValue("queue_tooltips").toBool()) return;

  QTreeWidgetItem* item = itemAt(event->pos());

  if (!item) {
    if (m_status != 0) {
      m_status->deleteLater();
      m_status = 0;
    }
  } else {
    Queue* q = static_cast<Queue*>(item->data(0, Qt::UserRole).value<void*>());

    if (!m_status || q != m_status->getQueue()) {
      if (m_status) m_status->deleteLater();
      m_status = new QueueToolTip(getMainWindow(), q);
      connect(m_status, SIGNAL(destroyed(QObject*)), this,
              SLOT(tooltipDestroyed(QObject*)));
    }

    m_status->move(mapToGlobal(event->pos()) + QPoint(25, 25));
    if (!m_status->isVisible()) {
      m_status->fill();
      m_status->show();
    }
  }
}

void QueueView::tooltipDestroyed(QObject* obj) {
  if (m_status == obj) m_status = 0;
}

void QueueView::setCurrentRow(int row) { setCurrentItem(topLevelItem(row)); }
