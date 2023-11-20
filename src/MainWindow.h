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

#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>

#include "DropBox.h"
#include "LogManager.h"
#include "MyTrayIcon.h"
#include "TransfersModel.h"
#include "captcha/CaptchaQt.h"
#include "config.h"
#include "ui_MainWindow.h"

class SpeedGraph;
class NewTransferDlg;
class ClipboardMonitor;
class ExtensionMgr;
class ClickableLabel;

class MainWindow : public QMainWindow, public Ui_MainWindow {
  Q_OBJECT
 public:
  MainWindow(bool bStartHidden);
  ~MainWindow();

  void setupUi();
  void move(int i);
  void changeAll(bool resume);
  QList<int> getSelection();
  void applySettings();
  void applySystemTheme();
  void loadCSS();
 public slots:
  void about();
  void showSettings();
  void saveWindowState();

  void updateUi();
  void newQueue();
  void deleteQueue();
  void addTransfer(QString uri = QString(),
                   Transfer::Mode mode = Transfer::ModeInvalid,
                   QString className = QString(), int queue = -1);
  void deleteTransfer();
  void deleteTransferData();
  void refreshQueues();
  void refreshDetailsTab();

  void currentTabChanged(int newTab);

  void moveToTop();
  void moveUp();
  void moveDown();
  void moveToBottom();

  void toggleInfoBar(bool show);
  void hideAllInfoBars();

  void resumeAll();
  void stopAll();

  void resumeTransfer();
  void forcedResumeTransfer();
  void pauseTransfer();
  void setTransfer(Transfer::State state);
  void copyRemoteURI();

  void pauseAllTransfers();

  void transferOptions();
  void removeCompleted();

  void queueItemActivated();
  void queueItemProperties();
  void queueItemContext(const QPoint& pos);

  void transferItemActivated();
  void transferItemDoubleClicked(const QModelIndex&);
  void transferItemContext(const QPoint& pos);

  void transferOpenFile();
  void transferOpenDirectory();

  void displayDestroyed();
  void menuActionTriggered();

  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
  void downloadStateChanged(Transfer* d, Transfer::State prev,
                            Transfer::State now);
  void downloadModeChanged(Transfer* d, Transfer::Mode prev,
                           Transfer::Mode now);

  void reconfigure();
  void showWindow(bool);

  void showHelp();
  void reportBug();
  void filterTextChanged(const QString& text);
#ifdef WITH_JPLUGINS
  void showPremiumStatus();
  void premiumStatusClosed();
  void updatesChecked();
  void updateBubbleManuallyClosed();
#endif
 protected:
  virtual void resizeEvent(QResizeEvent* event);
  QString getFilterText() const { return filterLineEdit->text(); }

  int getSelectedQueue();
  static Queue* getQueue(int index, bool lock = true);
  Queue* getCurrentQueue(bool lock = true);
  void doneQueue(Queue* q, bool unlock = true, bool refresh = true);
  static void applySystemIcon(QAction* action, QString path);

  virtual void closeEvent(QCloseEvent* event);
  virtual void hideEvent(QHideEvent* event);
  virtual void dragEnterEvent(QDragEnterEvent* event);
  virtual void dropEvent(QDropEvent* event);

  void restoreWindowState(bool bStartHidden);
  void connectActions();
  void fillSettingsMenu();
  void setupTrayIconMenu();

  void showTrayIcon();
  void transferOpen(bool bOpenFile);
  void initAppTools(QMenu* menu);

  static QPixmap grayscalePixmap(QPixmap in);

 private:
  QTimer* m_timer;
  MyTrayIcon m_trayIcon;
  QMenu m_trayIconMenu;
  TransfersModel* m_modelTransfers;
  QLabel m_labelStatus;
  QObject* m_pDetailsDisplay;
  LogManager* m_log;
  DropBox* m_dropBox;
  Transfer* m_lastTransfer;
  NewTransferDlg* m_dlgNewTransfer;
  QLineEdit* filterLineEdit;
  CaptchaQt m_captcha;
#ifdef WITH_JPLUGINS
  ClickableLabel* m_premiumAccounts;
  ClickableLabel* m_updates;
  ExtensionMgr* m_extensionMgr;
  QTimer m_extensionCheckTimer;
  bool m_bUpdatesBubbleManuallyClosed;
#endif

  QList<QAction*> m_menuActionObjects;
  ClipboardMonitor* m_clipboardMonitor;

 public:
  int m_nStatusWidgetsLeft, m_nStatusWidgetsRight;

  friend class DropBox;
  friend class SpeedLimitWidget;
  friend class RightClickLabel;
  friend class TransfersModel;
};

#endif
