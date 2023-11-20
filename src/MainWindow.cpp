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

#include "MainWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QUrl>
#include <QtDebug>

#include "AboutDlg.h"
#include "AppTools.h"
#include "AutoActionForm.h"
#include "ClickableLabel.h"
#include "ClipboardMonitor.h"
#include "CommentForm.h"
#include "DropBox.h"
#include "GenericOptsForm.h"
#include "InfoBar.h"
#include "NewTransferDlg.h"
#include "Queue.h"
#include "QueueDlg.h"
#include "QueueMgr.h"
#include "RuntimeException.h"
#include "Settings.h"
#include "SettingsDlg.h"
#include "SimpleEmail.h"
#include "SpeedGraph.h"
#include "SpeedLimitWidget.h"
#include "WidgetHostDlg.h"
#include "config.h"
#include "engines/FakeDownload.h"
#include "fatrat.h"
#include "filterlineedit.h"
#include "tools/HashDlg.h"
#include "util/BalloonTip.h"

#ifdef WITH_JPLUGINS
#include "engines/JavaAccountStatusWidget.h"
#include "engines/SettingsJavaPluginForm.h"
#include "java/JVM.h"
#include "ExtensionMgr.h"
#endif

#ifdef WITH_DOCUMENTATION
#include "tools/HelpBrowser.h"
#endif

// #include <stdexcept>
#include <climits>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;
extern QVector<SettingsItem> g_settingsPages;

static QList<MenuAction> m_menuActions;

// status widgets that need to be added after the main window is created
static QList<QPair<QWidget*, bool> > m_statusWidgets;

using namespace std;

MainWindow::MainWindow(bool bStartHidden)
    : m_timer(0),
      m_trayIcon(this),
      m_pDetailsDisplay(0),
      m_lastTransfer(0),
      m_dlgNewTransfer(0) {
  setupUi();
  restoreWindowState(bStartHidden && m_trayIcon.isVisible());

  applySettings();

  refreshQueues();
  if (g_queues.size()) {
    int sel = g_settings->value("state/selqueue", -1).toInt();
    if (sel < 0 || sel >= g_queues.size()) sel = 0;
    treeQueues->setCurrentRow(sel);
    queueItemActivated();
  }

  updateUi();

  m_clipboardMonitor = new ClipboardMonitor;
}

MainWindow::~MainWindow() {
  saveWindowState();

  delete m_modelTransfers;
  delete m_clipboardMonitor;
}

void MainWindow::setupUi() {
  bool useSystemTheme;
  Ui_MainWindow::setupUi(this);

  m_dropBox = new DropBox(this);

  m_modelTransfers = new TransfersModel(this);
  treeTransfers->setModel(m_modelTransfers);
  treeTransfers->setItemDelegate(new ProgressDelegate(treeTransfers));

  m_trayIcon.setIcon(QIcon(":/fatrat/fatrat.png"));
  m_trayIcon.setToolTip("FatRat");
  showTrayIcon();

  statusbar->addWidget(&m_labelStatus);
  statusbar->addPermanentWidget(new SpeedLimitWidget(statusbar));

  m_nStatusWidgetsLeft = m_nStatusWidgetsRight = 1;

  m_log = new LogManager(this, textTransferLog, textGlobalLog);

  useSystemTheme = getSettingsValue("gui/systemtheme").toBool();

#ifdef WITH_DOCUMENTATION
  QAction* action;
  QIcon xicon;
  if (useSystemTheme)
    xicon = QIcon::fromTheme("help-contents", QIcon(":/menu/about.png"));
  else
    xicon = QIcon(":/menu/about.png");
  action = new QAction(xicon, tr("Help"), menuHelp);
  menuHelp->insertAction(actionAboutQt, action);
  menuHelp->insertSeparator(actionAboutQt);
  connect(action, SIGNAL(triggered()), this, SLOT(showHelp()));
#endif

  QHeaderView* hdr = treeQueues->header();
  treeQueues->setColumnCount(1);
  hdr->hide();

  if (getSettingsValue("css").toBool()) loadCSS();

#ifdef WITH_JPLUGINS
  if (JVM::JVMAvailable()) {
    m_premiumAccounts = new ClickableLabel(this);

    m_premiumAccounts->setToolTip(tr("Premium account status..."));
    m_premiumAccounts->setScaledContents(true);
    m_premiumAccounts->setPixmap(QPixmap(":/fatrat/premium.png"));
    m_premiumAccounts->setMaximumSize(16, 16);
    m_premiumAccounts->setCursor(Qt::PointingHandCursor);
    statusbar->insertPermanentWidget(1, m_premiumAccounts);
    m_premiumAccounts->show();

    m_updates = new ClickableLabel(this);
    m_updates->setScaledContents(true);
    m_updates->setToolTip(tr("Extension updates: %1").arg(0));
    m_updates->setPixmap(QPixmap(":/fatrat/updates.png"));
    m_updates->setMaximumSize(16, 16);
    m_updates->setCursor(Qt::PointingHandCursor);
    statusbar->insertPermanentWidget(1, m_updates);
    m_updates->show();

    m_extensionMgr = new ExtensionMgr;
    connect(&m_extensionCheckTimer, SIGNAL(timeout()), m_extensionMgr,
            SLOT(loadFromServer()));
    connect(m_extensionMgr, SIGNAL(loaded()), this, SLOT(updatesChecked()));

    m_bUpdatesBubbleManuallyClosed = false;
    m_extensionCheckTimer.setInterval(60 * 60 * 1000);

    if (getSettingsValue("java/check_updates").toBool()) {
      m_extensionCheckTimer.start();
      QTimer::singleShot(20 * 1000, m_extensionMgr, SLOT(loadFromServer()));
    }

    connect(m_updates, SIGNAL(clicked()), this, SLOT(showSettings()));
  } else
    m_premiumAccounts = nullptr;
#endif

  connectActions();
  setupTrayIconMenu();

  for (int i = 0; i < m_statusWidgets.size(); i++) {
    if (m_statusWidgets[i].second)
      statusbar->insertPermanentWidget(m_nStatusWidgetsRight++,
                                       m_statusWidgets[i].first);
    else
      statusbar->insertWidget(m_nStatusWidgetsLeft++, m_statusWidgets[i].first);
    m_statusWidgets[i].first->show();
  }

  filterLineEdit = new Utils::FilterLineEdit(labelTransfers);
  QSize ss = labelTransfers->size();

  filterLineEdit->move(ss.width() - 2 - 200, 2);
  filterLineEdit->resize(200, ss.height() - 12);
  connect(filterLineEdit, SIGNAL(textChanged(const QString&)), this,
          SLOT(filterTextChanged(const QString&)));
  filterLineEdit->setFrame(false);
  filterLineEdit->show();

  fillSettingsMenu();
  if (useSystemTheme) applySystemTheme();
}

void MainWindow::setupTrayIconMenu() {
  m_trayIconMenu.addAction(actionDisplay);
  m_trayIconMenu.addAction(actionDropBox);
  m_trayIconMenu.addSeparator();
  m_trayIconMenu.addAction(actionPauseAll);
  m_trayIconMenu.addSeparator();
  m_trayIconMenu.addAction(actionHideAllInfoBars);
  m_trayIconMenu.addSeparator();
  m_trayIconMenu.addAction(actionQuit);

  m_trayIcon.setContextMenu(&m_trayIconMenu);
}

void MainWindow::resizeEvent(QResizeEvent* event) {
  if (event) QMainWindow::resizeEvent(event);
  QSize ss = labelTransfers->size();
  filterLineEdit->move(ss.width() - 2 - 200, 2);
}

void MainWindow::applySystemTheme() {
  applySystemIcon(actionResume, "media-playback-start");
  applySystemIcon(actionForcedResume, "media-seek-forward");
  applySystemIcon(actionPause, "media-playback-pause");
  applySystemIcon(actionTop, "go-top");
  applySystemIcon(actionUp, "go-up");
  applySystemIcon(actionDown, "go-down");
  applySystemIcon(actionBottom, "go-bottom");
  applySystemIcon(actionNewTransfer, "list-add");
  applySystemIcon(actionDeleteTransfer, "edit-delete");
  // applySystemIcon(actionDeleteTransferData, "edit-delete");
  applySystemIcon(actionQuit, "application-exit");
  applySystemIcon(actionProperties, "document-properties");
  applySystemIcon(actionQueueProperties, "document-properties");
  // applySystemIcon(actionNewQueue, "window-new");
  // applySystemIcon(actionDeleteQueue, "window-close");
  // applySystemIcon(actionRemoveCompleted, "tools-check-spelling");
  applySystemIcon(actionSettings, "preferences-other");
}

void MainWindow::applySystemIcon(QAction* action, QString path) {
  QIcon icon = QIcon::fromTheme(path);
  if (!icon.isNull()) action->setIcon(icon);
}

void MainWindow::fillSettingsMenu() {
  menuSettings->addSeparator();
  for (int i = 1; i < g_settingsPages.size(); i++) {
    QAction* act;
    act = menuSettings->addAction(g_settingsPages[i].icon,
                                  g_settingsPages[i].title);
    act->setData(i);
    connect(act, SIGNAL(triggered()), this, SLOT(showSettings()));
  }
}

void MainWindow::loadCSS() {
  QFile file;
  if (openDataFile(&file, "/data/css/label-headers.css")) {
    QByteArray data = file.readAll();
    labelQueues->setStyleSheet(data);
    labelTransfers->setStyleSheet(data);
    labelTransferLog->setStyleSheet(data);
    labelGlobalLog->setStyleSheet(data);
  }
}

void MainWindow::connectActions() {
  connect(actionAbout, SIGNAL(triggered()), this, SLOT(about()));
  connect(actionNewQueue, SIGNAL(triggered()), this, SLOT(newQueue()));
  connect(actionDeleteQueue, SIGNAL(triggered()), this, SLOT(deleteQueue()));
  connect(actionQueueProperties, SIGNAL(triggered()), this,
          SLOT(queueItemProperties()));

  connect(treeQueues,
          SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this,
          SLOT(queueItemActivated()));
  connect(treeQueues, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this,
          SLOT(queueItemActivated()));
  connect(treeQueues, SIGNAL(itemActivated(QTreeWidgetItem*, int)), this,
          SLOT(queueItemActivated()));
  connect(treeQueues, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this,
          SLOT(queueItemProperties()));
  connect(treeQueues, SIGNAL(customContextMenuRequested(const QPoint&)), this,
          SLOT(queueItemContext(const QPoint&)));

  connect(treeTransfers, SIGNAL(doubleClicked(const QModelIndex&)), this,
          SLOT(transferItemDoubleClicked(const QModelIndex&)));
  connect(treeTransfers, SIGNAL(customContextMenuRequested(const QPoint&)),
          this, SLOT(transferItemContext(const QPoint&)));

  QItemSelectionModel* model = treeTransfers->selectionModel();
  connect(
      model,
      SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
      this, SLOT(updateUi()));

  connect(actionNewTransfer, SIGNAL(triggered()), this, SLOT(addTransfer()));
  connect(actionDeleteTransfer, SIGNAL(triggered()), this,
          SLOT(deleteTransfer()));
  connect(actionDeleteTransferData, SIGNAL(triggered()), this,
          SLOT(deleteTransferData()));
  connect(actionRemoveCompleted, SIGNAL(triggered()), this,
          SLOT(removeCompleted()));

  connect(actionResume, SIGNAL(triggered()), this, SLOT(resumeTransfer()));
  connect(actionForcedResume, SIGNAL(triggered()), this,
          SLOT(forcedResumeTransfer()));
  connect(actionPause, SIGNAL(triggered()), this, SLOT(pauseTransfer()));

  connect(actionTop, SIGNAL(triggered()), this, SLOT(moveToTop()));
  connect(actionUp, SIGNAL(triggered()), this, SLOT(moveUp()));
  connect(actionDown, SIGNAL(triggered()), this, SLOT(moveDown()));
  connect(actionBottom, SIGNAL(triggered()), this, SLOT(moveToBottom()));
  connect(actionResumeAll, SIGNAL(triggered()), this, SLOT(resumeAll()));
  connect(actionStopAll, SIGNAL(triggered()), this, SLOT(stopAll()));

  connect(actionDropBox, SIGNAL(toggled(bool)), m_dropBox,
          SLOT(setVisible(bool)));
  connect(actionInfoBar, SIGNAL(toggled(bool)), this,
          SLOT(toggleInfoBar(bool)));
  connect(actionProperties, SIGNAL(triggered()), this, SLOT(transferOptions()));
  connect(actionDisplay, SIGNAL(toggled(bool)), this, SLOT(showWindow(bool)));
  connect(actionHideAllInfoBars, SIGNAL(triggered()), this,
          SLOT(hideAllInfoBars()));

  connect(actionOpenFile, SIGNAL(triggered()), this, SLOT(transferOpenFile()));
  connect(actionOpenDirectory, SIGNAL(triggered()), this,
          SLOT(transferOpenDirectory()));

  connect(pushGenericOptions, SIGNAL(clicked()), this, SLOT(transferOptions()));
  connect(tabMain, SIGNAL(currentChanged(int)), this,
          SLOT(currentTabChanged(int)));

  connect(actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
  connect(actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
  connect(&m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
          this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
  connect(actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));

  connect(
      TransferNotifier::instance(),
      SIGNAL(stateChanged(Transfer*, Transfer::State, Transfer::State)), this,
      SLOT(downloadStateChanged(Transfer*, Transfer::State, Transfer::State)));
  connect(TransferNotifier::instance(),
          SIGNAL(modeChanged(Transfer*, Transfer::Mode, Transfer::Mode)), this,
          SLOT(downloadModeChanged(Transfer*, Transfer::Mode, Transfer::Mode)));
  connect(actionBugReport, SIGNAL(triggered()), this, SLOT(reportBug()));
  connect(actionCopyRemoteURI, SIGNAL(triggered()), this,
          SLOT(copyRemoteURI()));

  connect(actionPauseAll, SIGNAL(triggered()), this, SLOT(pauseAllTransfers()));

#ifdef WITH_JPLUGINS
  if (m_premiumAccounts) {
    connect(m_premiumAccounts, SIGNAL(clicked()), this,
            SLOT(showPremiumStatus()));
  }
#endif
}

void MainWindow::restoreWindowState(bool bStartHidden) {
  QHeaderView* hdr = treeTransfers->header();
  QVariant state = g_settings->value("state/mainheaders");

  if (state.isNull())
    hdr->resizeSection(0, 300);
  else
    hdr->restoreState(state.toByteArray());

  state = g_settings->value("state/mainsplitter");
  if (state.isNull())
    splitterQueues->setSizes(QList<int>() << 80 << 600);
  else
    splitterQueues->restoreState(state.toByteArray());

  state = g_settings->value("state/statssplitter");
  if (!state.isNull()) splitterStats->restoreState(state.toByteArray());

  connect(hdr, SIGNAL(sectionResized(int, int, int)), this,
          SLOT(saveWindowState()));
  connect(splitterQueues, SIGNAL(splitterMoved(int, int)), this,
          SLOT(saveWindowState()));

  QPoint pos = g_settings->value("state/mainwindow_pos").toPoint();
  QSize size = g_settings->value("state/mainwindow_size").toSize();

  if (size.isEmpty()) {
    qDebug() << "Maximizing the main window";
    if (!bStartHidden) showMaximized();
  } else {
    QWidget::move(pos);
    resize(size);
    if (!bStartHidden) show();
  }

  if (bStartHidden) actionDisplay->setChecked(false);
  resizeEvent(0);
}

void MainWindow::saveWindowState() {
  qDebug() << "saveWindowState()";

  g_settings->setValue("state/mainheaders",
                       treeTransfers->header()->saveState());
  g_settings->setValue("state/mainsplitter", splitterQueues->saveState());
  g_settings->setValue("state/statssplitter", splitterStats->saveState());
  g_settings->setValue("state/mainwindow", saveGeometry());
  g_settings->setValue("state/selqueue", getSelectedQueue());

  g_settings->setValue("state/mainwindow_pos", pos());
  g_settings->setValue("state/mainwindow_size", size());

  g_settings->sync();
}

void MainWindow::about() { AboutDlg(this).exec(); }

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger) {
    if (!getSettingsValue("gui/hideunfocused").toBool() &&
        actionDisplay->isChecked() && !this->isActiveWindow()) {
      activateWindow();
      raise();
    } else
      actionDisplay->toggle();
  } else if (reason == QSystemTrayIcon::MiddleClick ||
             reason == QSystemTrayIcon::Context) {
    actionDropBox->toggle();
  }
}

void MainWindow::hideAllInfoBars() { InfoBar::removeAll(); }

void MainWindow::hideEvent(QHideEvent* event) {
  if (isMinimized() && m_trayIcon.isVisible() &&
      g_settings->value("hideminimize", getSettingsDefault("hideminimize"))
          .toBool()) {
    actionDisplay->setChecked(false);
    hide();
  } else
    QWidget::hideEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (m_trayIcon.isVisible() &&
      g_settings->value("hideclose", getSettingsDefault("hideclose"))
          .toBool()) {
    actionDisplay->setChecked(false);
    hide();
  } else {
    m_trayIcon.hide();
    QWidget::closeEvent(event);
    qApp->quit();
  }
}

void MainWindow::updateUi() {
  QList<int> sel = getSelection();
  Queue* q = getCurrentQueue();

  // queue view
  if (q != 0) {
    actionDeleteQueue->setEnabled(true);
    actionQueueProperties->setEnabled(true);
    actionNewTransfer->setEnabled(true);
    actionStopAll->setEnabled(true);
    actionResumeAll->setEnabled(true);

    speedGraphQueue->setRenderSource(q);
  } else {
    actionDeleteQueue->setEnabled(false);
    actionQueueProperties->setEnabled(false);
    actionNewTransfer->setEnabled(false);
    actionStopAll->setEnabled(false);
    actionResumeAll->setEnabled(false);
  }

  // transfer view
  Transfer* d = 0;
  int currentTab = tabMain->currentIndex();

  bool hasFilter = !getFilterText().isEmpty();

  if (!sel.empty()) {
    int rcount = m_modelTransfers->rowCount();
    bool bSingle = sel.size() == 1;

    if (!bSingle && (currentTab == 1 || currentTab == 2)) {
      tabMain->setCurrentIndex(0);
      currentTab = 0;
    }

    tabMain->setTabEnabled(1, bSingle);  // transfer details
    tabMain->setTabEnabled(2, bSingle);  // transfer graph

    actionOpenFile->setEnabled(bSingle);
    actionOpenDirectory->setEnabled(bSingle);

    if (bSingle) {
      actionTop->setEnabled(sel[0] && !hasFilter);
      actionBottom->setEnabled(sel[0] < rcount - 1 && !hasFilter);

      q->lock();

      d = q->at(sel[0]);
      actionInfoBar->setChecked(InfoBar::getInfoBar(d) != 0);

      Transfer::State state = d->state();
      actionForcedResume->setEnabled(Transfer::ForcedActive != state);
      actionResume->setEnabled(Transfer::Active != state);
      actionPause->setEnabled(Transfer::Paused != state);

      speedGraph->setRenderSource(d);
      m_log->setLogSource(d);

      q->unlock();
    } else {
      actionForcedResume->setEnabled(true);
      actionResume->setEnabled(true);
      actionPause->setEnabled(true);

      actionTop->setEnabled(!hasFilter);
      // actionUp->setEnabled(false);
      // actionDown->setEnabled(false);
      actionBottom->setEnabled(!hasFilter);
    }

    actionUp->setEnabled(!hasFilter);
    actionDown->setEnabled(!hasFilter);

    actionDeleteTransfer->setEnabled(true);
    actionDeleteTransferData->setEnabled(true);

    actionInfoBar->setEnabled(bSingle);
    actionProperties->setEnabled(bSingle);
  } else {
    if (currentTab == 1 || currentTab == 2) {
      tabMain->setCurrentIndex(0);
      currentTab = 0;
    }

    tabMain->setTabEnabled(1, false);  // transfer details
    tabMain->setTabEnabled(2, false);  // transfer graph

    actionOpenFile->setEnabled(false);
    actionOpenDirectory->setEnabled(false);

    actionTop->setEnabled(false);
    actionUp->setEnabled(false);
    actionDown->setEnabled(false);
    actionBottom->setEnabled(false);
    actionDeleteTransfer->setEnabled(false);
    actionDeleteTransferData->setEnabled(false);

    actionForcedResume->setEnabled(false);
    actionResume->setEnabled(false);
    actionPause->setEnabled(false);

    actionInfoBar->setEnabled(false);
    actionProperties->setEnabled(false);

    speedGraph->setRenderSource((Queue*)NULL);
    m_log->setLogSource(0);
  }

  if (d != m_lastTransfer) {
    m_lastTransfer = d;
    transferItemActivated();
  }

  actionNewTransfer->setEnabled(q != 0);
  actionRemoveCompleted->setEnabled(q != 0);

  if (q != 0) doneQueue(q, true, false);

  m_modelTransfers->refresh();
  if (currentTab == 1) refreshDetailsTab();
}

void MainWindow::refreshQueues() {
  g_queuesLock.lockForRead();

  int i;
  for (i = 0; i < g_queues.size(); i++) {
    QTreeWidgetItem* item;

    if (i >= treeQueues->topLevelItemCount()) {
      item = new QTreeWidgetItem(treeQueues, QStringList(g_queues[i]->name()));
      item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDropEnabled |
                     Qt::ItemIsEnabled);
      treeQueues->addTopLevelItem(item);
    } else {
      item = treeQueues->topLevelItem(i);
      item->setText(0, g_queues[i]->name());
    }

    item->setData(0, Qt::UserRole, QVariant::fromValue((void*)g_queues[i]));
  }

  while (i < treeQueues->topLevelItemCount())
    qDebug() << "Removing item" << i << treeQueues->takeTopLevelItem(i);

  int upt = 0, downt = 0;
  int upq = 0, downq = 0;
  int cur = getSelectedQueue();

  for (int j = 0; j < g_queues.size(); j++) {
    Queue* q = g_queues[j];
    q->lock();

    for (int i = 0; i < q->size(); i++) {
      int up, down;
      q->at(i)->speeds(down, up);
      downt += down;
      upt += up;

      if (j == cur) {
        downq += down;
        upq += up;
      }
    }

    q->unlock();
  }

  g_queuesLock.unlock();

  m_labelStatus.setText(QString(tr("Queue's speed: %1 down, %2 up"))
                            .arg(formatSize(downq, true))
                            .arg(formatSize(upq, true)));
}

void MainWindow::newQueue() {
  QueueDlg dlg(this);

  if (dlg.exec() == QDialog::Accepted) {
    Queue* q = new Queue;

    q->setName(dlg.m_strName);
    q->setSpeedLimits(dlg.m_nDownLimit, dlg.m_nUpLimit);
    if (dlg.m_bLimit)
      q->setTransferLimits(dlg.m_nDownTransfersLimit, dlg.m_nUpTransfersLimit);
    else
      q->setTransferLimits(-1, -1);
    q->setUpAsDown(dlg.m_bUpAsDown);
    q->setDefaultDirectory(dlg.m_strDefaultDirectory);
    q->setMoveDirectory(dlg.m_strMoveDirectory);

    g_queuesLock.lockForWrite();
    g_queues << q;
    g_queuesLock.unlock();

    Queue::saveQueuesAsync();
    refreshQueues();
  }
}

void MainWindow::deleteQueue() {
  int queue;

  queue = getSelectedQueue();

  if (g_queues.empty() || queue < 0) return;

  if (QMessageBox::warning(this, tr("Delete queue"),
                           tr("Do you really want to delete the active queue?"),
                           QMessageBox::Yes | QMessageBox::No) ==
      QMessageBox::Yes) {
    g_queuesLock.lockForWrite();
    delete g_queues.takeAt(queue);
    g_queuesLock.unlock();

    Queue::saveQueuesAsync();
    refreshQueues();
  }
}

void MainWindow::queueItemActivated() {
  updateUi();
  treeTransfers->selectionModel()->clearSelection();
  m_modelTransfers->setQueue(getSelectedQueue());

  if (m_pDetailsDisplay)
    m_pDetailsDisplay->deleteLater();
  else
    displayDestroyed();
}

void MainWindow::queueItemProperties() {
  QueueDlg dlg(this);

  Queue* q = getCurrentQueue(false);

  dlg.m_strName = q->name();
  dlg.m_bUpAsDown = q->upAsDown();
  dlg.m_strDefaultDirectory = q->defaultDirectory();
  dlg.m_strMoveDirectory = q->moveDirectory();
  q->speedLimits(dlg.m_nDownLimit, dlg.m_nUpLimit);
  q->transferLimits(dlg.m_nDownTransfersLimit, dlg.m_nUpTransfersLimit);

  if (dlg.m_nDownTransfersLimit < 0 || dlg.m_nUpTransfersLimit < 0) {
    dlg.m_nDownTransfersLimit = dlg.m_nUpTransfersLimit = 1;
    dlg.m_bLimit = false;
  } else
    dlg.m_bLimit = true;

  if (dlg.exec() == QDialog::Accepted) {
    q->setName(dlg.m_strName);
    q->setSpeedLimits(dlg.m_nDownLimit, dlg.m_nUpLimit);
    if (dlg.m_bLimit)
      q->setTransferLimits(dlg.m_nDownTransfersLimit, dlg.m_nUpTransfersLimit);
    else
      q->setTransferLimits();
    q->setUpAsDown(dlg.m_bUpAsDown);
    q->setDefaultDirectory(dlg.m_strDefaultDirectory);
    q->setMoveDirectory(dlg.m_strMoveDirectory);
    treeQueues->currentItem()->setText(0, dlg.m_strName);

    Queue::saveQueuesAsync();
  }

  doneQueue(q, false);
}

void MainWindow::queueItemContext(const QPoint&) {
  if (getSelectedQueue() != -1) {
    QMenu menu(treeQueues);

    menu.addAction(actionResumeAll);
    menu.addAction(actionStopAll);
    menu.addSeparator();
    menu.addAction(actionQueueProperties);

    updateUi();
    menu.exec(QCursor::pos());
  }
}

void MainWindow::transferItemActivated() {
  // updateUi();

  if (m_pDetailsDisplay)
    m_pDetailsDisplay->deleteLater();
  else
    displayDestroyed();
}

void MainWindow::displayDestroyed() {
  Queue* q = getCurrentQueue();
  // QModelIndex i = treeTransfers->currentIndex();
  QList<int> sel = getSelection();
  Transfer* d = 0;

  if (q != 0 && sel.size() == 1) d = q->at(sel[0]);

  if (QWidget* w = stackedDetails->currentWidget()) {
    // disconnect(w, SIGNAL(destroyed()), this, SLOT(displayDestroyed()));
    stackedDetails->removeWidget(w);
    delete w;
  }
  m_pDetailsDisplay = 0;

  if (d != 0) {
    QWidget* widgetDisplay = new QWidget(stackedDetails);
    m_pDetailsDisplay = d->createDetailsWidget(widgetDisplay);
    if (m_pDetailsDisplay) {
      stackedDetails->insertWidget(0, widgetDisplay);
      stackedDetails->setCurrentIndex(0);

      connect(m_pDetailsDisplay, SIGNAL(destroyed()), this,
              SLOT(displayDestroyed()));
    } else
      delete widgetDisplay;
  }

  doneQueue(q);
}

void MainWindow::transferItemDoubleClicked(const QModelIndex&) {
  int op = g_settings
               ->value("transfer_dblclk", getSettingsDefault("transfer_dblclk"))
               .toInt();

  switch (op) {
    case 0:
      tabMain->setCurrentIndex(1);
      break;
    case 1:
      tabMain->setCurrentIndex(2);
      break;
    case 2:
      transferOpenFile();
      break;
    case 3:
      transferOpenDirectory();
      break;
  }
}

void MainWindow::move(int i) {
  Queue* q = getCurrentQueue(false);
  QList<int> sel = getSelection();
  QModelIndex eVisible;

  if (!q) return;

  if (!sel.empty()) {
    QItemSelectionModel* model = treeTransfers->selectionModel();

    int size = sel.size();
    model->blockSignals(true);
    model->clearSelection();

    switch (i) {
      case 0: {
        for (int j = 0; j < size; j++) {
          q->moveToTop(sel[size - j - 1] + j);
          model->select(
              m_modelTransfers->index(j),
              QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }

        eVisible = m_modelTransfers->index(size - 1);
        break;
      }
      case 1:
        for (int i = 0; i < size; i++) {
          int newpos;

          newpos = q->moveUp(sel[i]);
          model->select(
              m_modelTransfers->index(newpos),
              QItemSelectionModel::Select | QItemSelectionModel::Rows);

          if (i == 0) eVisible = m_modelTransfers->index(newpos);
        }
        break;
      case 2:
        for (int i = size - 1; i >= 0; i--) {
          int newpos;

          newpos = q->moveDown(sel[i]);
          model->select(
              m_modelTransfers->index(newpos),
              QItemSelectionModel::Select | QItemSelectionModel::Rows);

          if (i == size - 1) eVisible = m_modelTransfers->index(newpos);
        }
        break;
      case 3: {
        int qsize = q->size();
        QItemSelectionModel* model = treeTransfers->selectionModel();

        for (int i = 0; i < size; i++) {
          q->moveToBottom(sel[i] - i);
          model->select(
              m_modelTransfers->index(qsize - i - 1),
              QItemSelectionModel::Select | QItemSelectionModel::Rows);

          if (i == 0) eVisible = m_modelTransfers->index(qsize - i - 1);
        }
        break;
      }
    }

    model->blockSignals(false);
  }

  doneQueue(q, false);
  Queue::saveQueuesAsync();

  treeTransfers->scrollTo(eVisible);
}

void MainWindow::moveToTop() { move(0); }

void MainWindow::moveUp() { move(1); }

void MainWindow::moveDown() { move(2); }

void MainWindow::moveToBottom() { move(3); }

void MainWindow::addTransfer(QString uri, Transfer::Mode mode,
                             QString className, int qSel) {
  Queue* queue = 0;

  if (m_dlgNewTransfer) {
    m_dlgNewTransfer->addLinks(uri);
    return;
  }
  if (qSel < 0) {
    qSel = getSelectedQueue();
    if (qSel < 0) {
      if (g_queues.size())
        qSel = 0;
      else
        return;
    }
  }

  m_dlgNewTransfer = new NewTransferDlg(this);

  m_dlgNewTransfer->setWindowTitle(tr("New transfer"));
  m_dlgNewTransfer->m_nQueue = qSel;
  m_dlgNewTransfer->m_strURIs = uri;

  if (!uri.isEmpty() && className.isEmpty()) {
    QStringList l = uri.split('\n', Qt::SkipEmptyParts);
    Transfer::BestEngine eng = Transfer::bestEngine(l[0], mode);

    if (eng.type != Transfer::ModeInvalid) m_dlgNewTransfer->m_mode = eng.type;
  } else {
    m_dlgNewTransfer->m_mode = mode;
    m_dlgNewTransfer->m_strClass = className;
  }

  QList<Transfer*> listTransfers;

show_dialog:
  try {
    QStringList uris;
    int sep = getSettingsValue("link_separator").toInt();

    if (m_dlgNewTransfer->exec() != QDialog::Accepted) throw RuntimeException();

    if (!sep)
      uris = m_dlgNewTransfer->m_strURIs.split('\n', Qt::SkipEmptyParts);
    else
      uris = m_dlgNewTransfer->m_strURIs.split(QRegExp("\\s+"),
                                               Qt::SkipEmptyParts);

    if (uris.isEmpty()) throw RuntimeException();

    for (int i = 0; i < uris.size(); i++) {
      QString trm = uris[i].trimmed();

      if (trm.isEmpty()) {
        uris.removeAt(i);
        i--;
      } else
        uris[i] = trm;
    }

    int detectedClass =
        m_dlgNewTransfer->m_nClass;  // used for the multiple cfg dialog
    for (int i = 0; i < uris.size(); i++) {
      Transfer* d;

      int classID;
      if (m_dlgNewTransfer->m_nClass == -1) {
        // autodetection
        Transfer::BestEngine eng;

        if (m_dlgNewTransfer->m_mode == Transfer::Download)
          eng = Transfer::bestEngine(uris[i], Transfer::Download);
        else
          eng = Transfer::bestEngine(m_dlgNewTransfer->m_strDestination,
                                     Transfer::Upload);

        if (eng.nClass < 0)
          throw RuntimeException(
              tr("Couldn't autodetect transfer type for \"%1\"").arg(uris[i]));
        else
          classID = eng.nClass;

        if (detectedClass == -1)
          detectedClass = classID;
        else if (detectedClass >= 0 && detectedClass != classID)
          detectedClass = -2;
      } else
        classID = m_dlgNewTransfer->m_nClass;

      d = Transfer::createInstance(m_dlgNewTransfer->m_mode, classID);

      if (d == 0)
        throw RuntimeException(tr("Failed to create a class instance."));

      listTransfers << d;

      QString source, destination;

      source = uris[i].trimmed();
      destination = m_dlgNewTransfer->m_strDestination;

      if (!m_dlgNewTransfer->m_auth.strUser.isEmpty()) {
        QString& obj = (m_dlgNewTransfer->m_mode == Transfer::Download)
                           ? source
                           : destination;

        QUrl url = obj;
        if (url.userInfo().isEmpty()) {
          url.setUserName(m_dlgNewTransfer->m_auth.strUser);
          url.setPassword(m_dlgNewTransfer->m_auth.strPassword);
        }
        obj = url.toString();
      }

      d->init(source, destination);
      d->setUserSpeedLimits(m_dlgNewTransfer->m_nDownLimit,
                            m_dlgNewTransfer->m_nUpLimit);
    }

    // show the transfer details dialog
    if (m_dlgNewTransfer->m_bDetails) {
      // show a typical transfer propeties dialog
      if (listTransfers.size() == 1) {
        WidgetHostDlg dlg(this);
        m_dlgNewTransfer->setWindowTitle(tr("Transfer details"));

        if (WidgetHostChild* q =
                listTransfers[0]->createOptionsWidget(dlg.getChildHost())) {
          dlg.addChild(q);

          if (dlg.exec() != QDialog::Accepted) throw RuntimeException();
        }
      } else if (detectedClass >= 0)  // show a dialog designed for multiple
      {
        if (!Transfer::runProperties(this, m_dlgNewTransfer->m_mode,
                                     detectedClass, listTransfers))
          throw RuntimeException();
      } else {
        QMessageBox::warning(this, "FatRat",
                             tr("Cannot display detailed configuration when "
                                "there are multiple transfer types used."));
      }
    }

    if (!m_dlgNewTransfer->m_bPaused) {
      foreach (Transfer* d, listTransfers) d->setState(Transfer::Waiting);
    }

    queue = getQueue(m_dlgNewTransfer->m_nQueue, false);

    if (!queue) throw RuntimeException(tr("Internal error."));

    queue->add(listTransfers);
  } catch (const RuntimeException& e) {
    qDeleteAll(listTransfers);
    listTransfers.clear();
    if (!e.what().isEmpty()) {
      QMessageBox::critical(this, tr("Error"), e.what());
      goto show_dialog;
    }
  }

  delete m_dlgNewTransfer;
  m_dlgNewTransfer = 0;

  if (queue != 0) doneQueue(queue, false);

  Queue::saveQueuesAsync();
}

void MainWindow::deleteTransfer() {
  Queue* q = getCurrentQueue(false);
  QList<int> sel = getSelection();

  if (!q) return;

  if (!sel.empty()) {
    if (QMessageBox::warning(
            this, tr("Delete transfers"),
            tr("Do you really want to delete selected transfers?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
      treeTransfers->selectionModel()->clearSelection();

      q->lockW();
      for (int i = 0; i < sel.size(); i++) q->remove(sel[i] - i, true);
      q->unlock();
      Queue::saveQueuesAsync();
    }
  }

  doneQueue(q, false);
}

void MainWindow::deleteTransferData() {
  Queue* q = getCurrentQueue(false);
  QList<int> sel = getSelection();

  if (!q) return;

  if (!sel.empty()) {
    if (QMessageBox::warning(this, tr("Delete transfers"),
                             tr("Do you really want to delete selected "
                                "transfers <b>including the data</b>?"),
                             QMessageBox::Yes | QMessageBox::No) ==
        QMessageBox::Yes) {
      treeTransfers->selectionModel()->clearSelection();

      q->lockW();
      // bool bOK = true;

      for (int i = 0; i < sel.size(); i++)
        /*bOK &=*/q->removeWithData(sel[i] - i, true);
      q->unlock();
      Queue::saveQueuesAsync();

      /*if(!bOK)
      {
              QMessageBox::warning(this, tr("Delete transfers"),
                      tr("FatRat failed to remove some files, check your
      permissions."));
      }*/
    }
  }

  doneQueue(q, false);
}

void MainWindow::resumeTransfer() {
  Queue* q = getCurrentQueue();
  QList<int> sel = getSelection();

  if (!q) return;

  foreach (int i, sel) {
    Transfer* d = q->at(i);
    Transfer::State state = d->state();

    if (state == Transfer::ForcedActive)
      d->setState(Transfer::Active);
    else if (state != Transfer::Active)
      d->setState(Transfer::Waiting);
  }

  doneQueue(q);
}

void MainWindow::forcedResumeTransfer() { setTransfer(Transfer::ForcedActive); }

void MainWindow::pauseTransfer() { setTransfer(Transfer::Paused); }

void MainWindow::setTransfer(Transfer::State state) {
  Queue* q = getCurrentQueue();
  QList<int> sel = getSelection();

  if (!q) return;

  foreach (int i, sel) {
    Transfer* d = q->at(i);
    if (d->state() != Transfer::Active || state != Transfer::Waiting)
      d->setState(state);
  }

  doneQueue(q);
}

void MainWindow::transferOptions() {
  WidgetHostDlg dlg(this);

  QList<int> sel = getSelection();
  Queue* q = getCurrentQueue();
  Transfer* d;

  if (!q) return;

  d = q->at(sel[0]);

  if (d != 0) {
    QWidget* widgetDetails;
    GenericOptsForm* wgt =
        new GenericOptsForm(dlg.getNextChildHost(tr("Generic options")));

    widgetDetails = dlg.getNextChildHost(tr("Details"));
    CommentForm* comment =
        new CommentForm(dlg.getNextChildHost(tr("Comment")), d);
    AutoActionForm* aaction =
        new AutoActionForm(dlg.getNextChildHost(tr("Actions")), d);

    wgt->m_mode = d->primaryMode();
    wgt->m_strURI = d->object();

    d->userSpeedLimits(wgt->m_nDownLimit, wgt->m_nUpLimit);
    wgt->m_nDownLimit /= 1024;
    wgt->m_nUpLimit /= 1024;

    dlg.setWindowTitle(tr("Transfer properties"));
    dlg.addChild(wgt);

    if (WidgetHostChild* c = d->createOptionsWidget(widgetDetails))
      dlg.addChild(c);
    else
      dlg.removeChildHost(widgetDetails);
    dlg.addChild(comment);
    dlg.addChild(aaction);

    if (dlg.exec() == QDialog::Accepted) {
      try {
        d->setObject(wgt->m_strURI);
      } catch (const RuntimeException& e) {
        QMessageBox::critical(this, tr("Error"), e.what());
      }
      d->setUserSpeedLimits(wgt->m_nDownLimit * 1024, wgt->m_nUpLimit * 1024);
      updateUi();
      Queue::saveQueuesAsync();
    }
  }
  doneQueue(q);
}

void MainWindow::refreshDetailsTab() {
  Queue* q;
  Transfer* d;
  QList<int> sel = getSelection();
  QString progress;

  if ((q = getCurrentQueue()) == 0) {
    tabMain->setCurrentIndex(0);
    return;
  }

  if (sel.size() != 1) {
    doneQueue(q, true, false);
    tabMain->setCurrentIndex(0);
    return;
  }

  d = q->at(sel[0]);

  lineName->setText(d->name());
  lineMessage->setText(d->message());
  lineDestination->setText(d->dataPath(false));

  if (d->total())
    progress = QString(tr("completed %1 from %2 (%3%)"))
                   .arg(formatSize(d->done()))
                   .arg(formatSize(d->total()))
                   .arg(100.0 / d->total() * d->done(), 0, 'f', 1);
  else
    progress = QString(tr("completed %1, total size unknown"))
                   .arg(formatSize(d->done()));

  if (d->isActive()) {
    int down, up;
    d->speeds(down, up);
    Transfer::Mode mode = d->primaryMode();
    QString speed;

    if (down)
      speed = QString("%1 kB/s down ").arg(double(down) / 1024.f, 0, 'f', 1);
    if (up) speed += QString("%1 kB/s up").arg(double(up) / 1024.f, 0, 'f', 1);

    if (d->total()) {
      QString s;
      qulonglong totransfer = d->total() - d->done();

      if (down && mode == Transfer::Download)
        progress += QString(tr(", %1 left")).arg(formatTime(totransfer / down));
      else if (up && mode == Transfer::Upload)
        progress += QString(tr(", %1 left")).arg(formatTime(totransfer / up));
    }

    lineSpeed->setText(speed);
  } else
    lineSpeed->setText(QString());

  lineProgress->setText(progress);
  lineRuntime->setText(formatTime(d->timeRunning()));

  doneQueue(q, true, false);
}

void MainWindow::currentTabChanged(int newTab) {
  if (newTab == 1) refreshDetailsTab();
}

void MainWindow::removeCompleted() {
  Queue* q;
  QString progress;

  if (getSelectedQueue() == -1) {
    tabMain->setCurrentIndex(0);
    return;
  }

  q = getCurrentQueue(false);
  q->lockW();

  for (int i = 0; i < q->size(); i++) {
    Transfer* d = q->at(i);
    if (d->state() == Transfer::Completed) q->remove(i--, true);
  }

  q->unlock();

  doneQueue(q, false, true);
}

void MainWindow::resumeAll() { changeAll(true); }

void MainWindow::stopAll() { changeAll(false); }

void MainWindow::changeAll(bool resume) {
  Queue* q = getCurrentQueue();
  if (!q) return;

  for (int i = 0; i < q->size(); i++) {
    Transfer::State state = q->at(i)->state();

    if ((state == Transfer::Paused || state == Transfer::Failed) && resume)
      q->at(i)->setState(Transfer::Waiting);
    else if (!resume && state != Transfer::Completed)
      q->at(i)->setState(Transfer::Paused);
  }

  doneQueue(q);
}

int MainWindow::getSelectedQueue() {
  QModelIndexList list = treeQueues->selectionModel()->selectedRows();

  if (list.isEmpty())
    return -1;
  else
    return list[0].row();
}

Queue* MainWindow::getQueue(int index, bool lock) {
  g_queuesLock.lockForRead();

  if (index < 0 || index >= g_queues.size()) {
    if (index != -1)
      qDebug() << "MainWindow::getQueue(): Invalid queue requested: " << index;
    g_queuesLock.unlock();
    return 0;
  }

  Queue* q = g_queues[index];
  if (lock) q->lock();
  return q;
}

Queue* MainWindow::getCurrentQueue(bool lock) {
  return getQueue(getSelectedQueue(), lock);
}

void MainWindow::doneQueue(Queue* q, bool unlock, bool refresh) {
  if (q != 0) {
    if (unlock) q->unlock();
    g_queuesLock.unlock();
    if (refresh) updateUi();
  }
}

void MainWindow::transferItemContext(const QPoint&) {
  QList<int> sel = getSelection();
  if (!sel.isEmpty()) {
    QMenu menu(treeTransfers);

    menu.addAction(actionOpenFile);
    menu.addAction(actionOpenDirectory);
    menu.addSeparator();
    menu.addAction(actionResume);
    menu.addAction(actionForcedResume);
    menu.addAction(actionPause);
    menu.addSeparator();
    menu.addAction(actionDeleteTransfer);
    menu.addAction(actionDeleteTransferData);
    menu.addSeparator();
    menu.addAction(actionTop);
    menu.addAction(actionUp);
    menu.addAction(actionDown);
    menu.addAction(actionBottom);
    menu.addSeparator();
    menu.addAction(actionCopyRemoteURI);
    menu.addSeparator();

    Queue* q = getCurrentQueue();
    if (q != 0) {
      if (sel.size() == 1) {
        Transfer* t = q->at(sel[0]);
        t->fillContextMenu(menu);
      }

      doneQueue(q, true, false);
    }

    menu.addSeparator();

    if (!m_menuActions.isEmpty()) {
      m_menuActionObjects.clear();
      foreach (MenuAction act, m_menuActions) {
        m_menuActionObjects << menu.addAction(act.icon, act.strName, this,
                                              SLOT(menuActionTriggered()));
      }
      menu.addSeparator();
    }

    menu.addAction(actionInfoBar);
    menu.addSeparator();
    menu.addAction(actionProperties);

    updateUi();
    menu.exec(QCursor::pos());
  }
}

void MainWindow::toggleInfoBar(bool show) {
  if (Queue* q = getCurrentQueue()) {
    QModelIndex ctrans = treeTransfers->currentIndex();
    Transfer* d = q->at(ctrans.row());

    if (d != 0) {
      InfoBar* bar = InfoBar::getInfoBar(d);
      if (show && !bar)
        new InfoBar(this, d);
      else if (!show)
        delete bar;
    }

    doneQueue(q, true, false);
  }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (event->mimeData()->hasFormat("text/plain")) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
  event->acceptProposedAction();
  addTransfer(event->mimeData()->text());
}

void MainWindow::showSettings() {
  SettingsDlg dlg(this);
  int index = 0;

  if (QAction* act = dynamic_cast<QAction*>(sender()))
    index = act->data().toInt();
#ifdef WITH_JPLUGINS
  if (dynamic_cast<ClickableLabel*>(sender()) ||
      dynamic_cast<BalloonTip*>(sender()))
    dlg.setPageByType<SettingsJavaPluginForm>();
#endif

  if (index) dlg.setPage(index);

  dlg.exec();
}

void MainWindow::showTrayIcon() {
  m_trayIcon.setVisible(
      g_settings->value("trayicon", getSettingsDefault("trayicon")).toBool());
}

void MainWindow::downloadStateChanged(Transfer* d, Transfer::State prev,
                                      Transfer::State now) {
  const bool popup =
      g_settings->value("showpopup", getSettingsDefault("showpopup")).toBool();
  const bool email =
      g_settings->value("sendemail", getSettingsDefault("sendemail")).toBool();

  if (actionPauseAll->isChecked() && now != Transfer::Paused)
    actionPauseAll->setChecked(false);

  if (!popup && !email) return;

  if (prev == Transfer::Active || prev == Transfer::ForcedActive) {
    if (now == Transfer::Completed) {
      if (popup) {
        m_trayIcon.showMessage(
            tr("Transfer completed"),
            QString(tr("The transfer of \"%1\" has been completed."))
                .arg(d->name()),
            QSystemTrayIcon::Information,
            g_settings->value("popuptime", getSettingsDefault("popuptime"))
                    .toInt() *
                1000);
      }
      if (email) {
        QString from, to;
        QString message;

        from =
            g_settings->value("emailsender", getSettingsDefault("emailsender"))
                .toString();
        to = g_settings->value("emailrcpt", getSettingsDefault("emailrcpt"))
                 .toString();

        message = QString(
                      "From: <%1>\r\nTo: <%2>\r\n"
                      "Subject: FatRat transfer completed\r\n"
                      "X-Mailer: FatRat/" VERSION
                      "\r\n"
                      "The transfer of \"%3\" has been completed.")
                      .arg(from)
                      .arg(to)
                      .arg(d->name());

        new SimpleEmail(
            g_settings->value("smtpserver", getSettingsDefault("smtpserver"))
                .toString(),
            from, to, message);
      }
    } else if (now == Transfer::Failed) {
      if (popup) {
        m_trayIcon.showMessage(
            tr("Transfer failed"),
            QString(tr("The transfer \"%1\" has failed.")).arg(d->name()),
            QSystemTrayIcon::Warning,
            g_settings->value("popuptime", getSettingsDefault("popuptime"))
                    .toInt() *
                1000);
      }
    }
  }
}

void MainWindow::downloadModeChanged(Transfer* d, Transfer::Mode prev,
                                     Transfer::Mode now) {
  const bool popup =
      g_settings->value("showpopup", getSettingsDefault("showpopup")).toBool();

  if (!popup) return;

  if (prev == Transfer::Download && now == Transfer::Upload) {
    m_trayIcon.showMessage(
        tr("Download completed"),
        QString(tr("The download of \"%1\" has been completed. Starting the "
                   "upload now."))
            .arg(d->name()),
        QSystemTrayIcon::Information,
        g_settings->value("popuptime", getSettingsDefault("popuptime"))
                .toInt() *
            1000);
  }
}

QList<int> MainWindow::getSelection() {
  QModelIndexList list = treeTransfers->selectionModel()->selectedRows();
  QList<int> result;

  if (Queue* q = getCurrentQueue()) {
    int size = qMin(q->size(), m_modelTransfers->rowCount());

    doneQueue(q, true, false);

    foreach (QModelIndex in, list) {
      int row = in.row();

      if (row < size) result << m_modelTransfers->remapIndex(row);
    }

    std::sort(result.begin(), result.end());
  }

  return result;
}

void MainWindow::transferOpen(bool bOpenFile) {
  QList<int> sel = getSelection();

  if (sel.size() != 1) return;

  if (Queue* q = getCurrentQueue()) {
    QString path;
    Transfer* d = q->at(sel[0]);
    QString obj = d->object();

    path = d->dataPath(bOpenFile);

    doneQueue(q, true, false);

    QProcess::startDetached(
        g_settings->value("fileexec", getSettingsDefault("fileexec"))
            .toString(),
        QStringList() << path);
  }
}

void MainWindow::transferOpenDirectory() { transferOpen(false); }

void MainWindow::transferOpenFile() { transferOpen(true); }

void MainWindow::reconfigure() {
  showTrayIcon();
  if (m_dropBox != 0) m_dropBox->reconfigure();
}

void MainWindow::showWindow(bool show) {
  if (show) {
    updateUi();
    refreshQueues();
    widgetStats->refresh();
  }

  if (show)
    showNormal();
  else
    hide();

  actionDisplay->setChecked(show);
}

void MainWindow::showHelp() {
#ifdef WITH_DOCUMENTATION
  QWidget* w = new HelpBrowser;
  connect(w, SIGNAL(changeTabTitle(QString)), tabMain,
          SLOT(changeTabTitle(QString)));
  tabMain->setCurrentIndex(
      tabMain->addTab(w, QIcon(":/menu/about.png"), tr("Help")));
#endif
}

void MainWindow::applySettings() {
  delete m_timer;
  m_timer = new QTimer(this);

  m_timer->start(getSettingsValue("gui_refresh").toInt());

  connect(m_timer, SIGNAL(timeout()), this, SLOT(updateUi()));
  connect(m_timer, SIGNAL(timeout()), this, SLOT(refreshQueues()));
  connect(m_timer, SIGNAL(timeout()), widgetStats, SLOT(refresh()));

#ifdef WITH_JPLUGINS
  bool now = m_extensionCheckTimer.isActive();
  if (getSettingsValue("java/check_updates").toBool()) {
    if (!now) m_extensionCheckTimer.start();
  } else {
    if (now) m_extensionCheckTimer.stop();
  }
#endif
}

void MainWindow::menuActionTriggered() {
  QAction* action = static_cast<QAction*>(sender());
  int i = m_menuActionObjects.indexOf(action);

  if (i < 0) return;

  Queue* q = getCurrentQueue();
  if (q != 0 && m_menuActions[i].lpfnTriggered) {
    QModelIndex ctrans = treeTransfers->currentIndex();
    Transfer* t = q->at(ctrans.row());

    m_menuActions[i].lpfnTriggered(t, q);

    doneQueue(q, true, false);
  }
}

void MainWindow::reportBug() {
  QProcess::startDetached("xdg-open",
                          QStringList() << QLatin1String(
                              "https://github.com/LubosD/fatrat/issues"));
}

void MainWindow::copyRemoteURI() {
  Queue* q = getCurrentQueue();
  QList<int> sel = getSelection();
  QString uris;

  if (!q) return;

  foreach (int i, sel) {
    Transfer* t = q->at(i);
    QString uri = t->remoteURI();

    if (!uri.isEmpty()) {
      if (!uris.isEmpty()) uris += '\n';
      uris += uri;
    }
  }

  doneQueue(q);
  QApplication::clipboard()->setText(uris);
}

void MainWindow::filterTextChanged(const QString& text) {
  treeTransfers->selectionModel()->clearSelection();
  updateUi();
}

void MainWindow::pauseAllTransfers() {
  if (actionPauseAll->isChecked())
    QueueMgr::instance()->pauseAllTransfers();
  else
    QueueMgr::instance()->unpauseAllTransfers();
}

#ifdef WITH_JPLUGINS
void MainWindow::showPremiumStatus() {
  JavaAccountStatusWidget* w = new JavaAccountStatusWidget(this);
  QPoint pos = mapFromGlobal(QCursor::pos());

  pos -= QPoint(290, 210);

  w->move(pos);
  w->show();

  m_premiumAccounts->setEnabled(false);
  connect(w, SIGNAL(destroyed()), this, SLOT(premiumStatusClosed()));
}

void MainWindow::premiumStatusClosed() { m_premiumAccounts->setEnabled(true); }

void MainWindow::updatesChecked() {
  QList<ExtensionMgr::PackageInfo> pkgs = m_extensionMgr->getPackages();
  int numUpdates = 0;

  for (int i = 0; i < pkgs.size(); i++) {
    qDebug() << pkgs[i].name << pkgs[i].installedVersion
             << pkgs[i].latestVersion;
    if (!pkgs[i].installedVersion.isEmpty() &&
        !pkgs[i].latestVersion.isEmpty() &&
        pkgs[i].installedVersion < pkgs[i].latestVersion) {
      numUpdates++;
    }
  }

  if (numUpdates) {
    if (!m_bUpdatesBubbleManuallyClosed && isVisible()) {
      BalloonTip* baloonTip =
          new BalloonTip(this, QIcon(), tr("Extension updates"),
                         tr("There are %1 updates available.").arg(numUpdates));
      baloonTip->balloon(m_updates->mapToGlobal(QPoint(8, 8)), 0);
      connect(baloonTip, SIGNAL(messageClicked()), this, SLOT(showSettings()));
      connect(baloonTip, SIGNAL(manuallyClosed()), this,
              SLOT(updateBubbleManuallyClosed()));
      m_updates->setToolTip(tr("Extension updates: %1").arg(numUpdates));
      m_updates->setPixmap(QPixmap(":/fatrat/updates.png"));
    }
  } else
    m_updates->setPixmap(grayscalePixmap(QPixmap(":/fatrat/updates.png")));
}

void MainWindow::updateBubbleManuallyClosed() {
  m_bUpdatesBubbleManuallyClosed = true;
}

#endif

QPixmap MainWindow::grayscalePixmap(QPixmap pixmap) {
  QImage image = pixmap.toImage();
  QRgb col;
  int gray;
  int width = pixmap.width();
  int height = pixmap.height();
  for (int i = 0; i < width; ++i) {
    for (int j = 0; j < height; ++j) {
      col = image.pixel(i, j);
      gray = qGray(col);
      image.setPixel(i, j, qRgba(gray, gray, gray, qAlpha(col)));
    }
  }
  return pixmap.fromImage(image);
}

void addMenuAction(const MenuAction& action) { m_menuActions << action; }

void addStatusWidget(QWidget* widget, bool bRight) {
  if (!widget) {
    qWarning() << "addStatusWidget called with nullptr widget. This is a bug!";
    return;
  }
  MainWindow* wnd = (MainWindow*)getMainWindow();

  if (wnd != 0) {
    if (bRight)
      wnd->statusbar->insertPermanentWidget(wnd->m_nStatusWidgetsRight++,
                                            widget);
    else
      wnd->statusbar->insertWidget(wnd->m_nStatusWidgetsLeft++, widget);
    widget->show();
  }
  m_statusWidgets << QPair<QWidget*, bool>(widget, bRight);
}

void removeStatusWidget(QWidget* widget) {
  MainWindow* wnd = (MainWindow*)getMainWindow();
  if (wnd != 0) {
    widget->hide();
    wnd->statusbar->removeWidget(widget);
  }

  for (int i = 0; i < m_statusWidgets.size(); i++) {
    if (m_statusWidgets[i].first != widget) continue;

    if (m_statusWidgets[i].second)
      wnd->m_nStatusWidgetsRight--;
    else
      wnd->m_nStatusWidgetsLeft--;

    m_statusWidgets.removeAt(i);
    break;
  }
}
