#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H
#include <QMainWindow>
#include "ui_MainWindow.h"
#include <QTimer>
#include <QSystemTrayIcon>
#include <QToolButton>
#include "TransfersModel.h"
#include "TransferLog.h"

class SpeedGraph;

class MainWindow : public QMainWindow, public Ui_MainWindow
{
Q_OBJECT
public:
	MainWindow();
	~MainWindow();
	
	void setupUi();
	void move(int i);
	void changeAll(bool resume);
	QList<int> getSelection();
public slots:
	void about();
	void showSettings();
	void saveWindowState();
	
	void updateUi();
	void newQueue();
	void deleteQueue();
	void addTransfer(QString uri = QString(), Transfer::Mode mode = Transfer::ModeInvalid, QString className = QString());
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
	
	void transferOptions();
	void removeCompleted();
	
	void queueItemActivated();
	void queueItemProperties();
	void queueItemContext(const QPoint& pos);
	
	void transferItemActivated(const QModelIndex&);
	void transferItemDoubleClicked(const QModelIndex&);
	void transferItemContext(const QPoint& pos);
	
	void transferOpenFile();
	void transferOpenDirectory();
	
	void displayDestroyed();
	
	void computeHash();
	
	void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
	void downloadStateChanged(Transfer* d, Transfer::State prev, Transfer::State now);
	void downloadModeChanged(Transfer* d, Transfer::State prev, Transfer::State now);
protected:
	int getSelectedQueue();
	static Queue* getQueue(int index, bool lock = true);
	Queue* getCurrentQueue(bool lock = true);
	void doneQueue(Queue* q, bool unlock = true, bool refresh = true);

	virtual void closeEvent(QCloseEvent* event);
	virtual void hideEvent(QHideEvent* event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);
	
	void restoreWindowState();
	void connectActions();
	
	void showTrayIcon();
	void transferOpen(bool bOpenFile);
	void initAppTools(QMenu* menu);
private:
	QTimer m_timer;
	QSystemTrayIcon m_trayIcon;
	TransfersModel* m_modelTransfers;
	QLabel m_labelStatus;
	QObject* m_pDetailsDisplay;
	SpeedGraph* m_graph;
	TransferLog* m_log;
	QWidget* m_dropBox;
	
	friend class DropBox;
	friend class SpeedLimitWidget;
	friend class RightClickLabel;
};

#endif
