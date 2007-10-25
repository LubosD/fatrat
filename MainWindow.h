#ifndef _MAINWINDOW_H
#define _MAINWINDOW_H
#include <QMainWindow>
#include "ui_MainWindow.h"
#include <QTimer>
#include <QSystemTrayIcon>
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
public slots:
	void about();
	void showSettings();
	void saveWindowState();
	
	void updateUi();
	void newQueue();
	void deleteQueue();
	void addTransfer(QString uri = QString());
	void deleteTransfer();
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
	
	void queueItemActivated(QListWidgetItem*);
	void queueItemProperties();
	void queueItemContext(const QPoint& pos);
	
	void transferItemActivated(const QModelIndex&);
	void transferItemDoubleClicked(const QModelIndex&);
	void transferItemContext(const QPoint& pos);
	
	void displayDestroyed();
	
	void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
	void downloadStateChanged(Transfer* d, Transfer::State prev, Transfer::State now);
	void downloadModeChanged(Transfer* d, Transfer::State prev, Transfer::State now);
private:
	Queue* getCurrentQueue(bool lock = true);
	void doneCurrentQueue(Queue* q, bool unlock = true, bool refresh = true);
	QList<int> getSelection();
protected:
	virtual void closeEvent(QCloseEvent* event);
	virtual void hideEvent(QHideEvent* event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);
	
	void showTrayIcon();
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
};

#endif
