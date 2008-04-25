#include "config.h"
#include "fatrat.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QtDebug>
#include <QProcess>
#include <QUrl>

#include "MainWindow.h"
#include "QueueDlg.h"
#include "Queue.h"
#include "engines/FakeDownload.h"
#include "WidgetHostDlg.h"
#include "NewTransferDlg.h"
#include "GenericOptsForm.h"
#include "InfoBar.h"
#include "SettingsDlg.h"
#include "SimpleEmail.h"
#include "SpeedGraph.h"
#include "DropBox.h"
#include "CommentForm.h"
#include "AutoActionForm.h"
#include "tools/HashDlg.h"
#include "RuntimeException.h"
#include "SpeedLimitWidget.h"
#include "AppTools.h"
#include "AboutDlg.h"

#ifdef WITH_DOCUMENTATION
#	include "tools/HelpBrowser.h"
#endif

#include <stdexcept>

extern QList<Queue*> g_queues;
extern QReadWriteLock g_queuesLock;
extern QSettings* g_settings;

using namespace std;

MainWindow::MainWindow(bool bStartHidden) : m_trayIcon(this), m_pDetailsDisplay(0), m_lastTransfer(0)
{
	setupUi();
	restoreWindowState(bStartHidden && m_trayIcon.isVisible());
	
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(updateUi()));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refreshQueues()));
	connect(&m_timer, SIGNAL(timeout()), widgetStats, SLOT(refresh()));
	
	m_timer.start(1000);
	
	refreshQueues();
	if(g_queues.size())
	{
		int sel = g_settings->value("state/selqueue", -1).toInt();
		if(sel < 0 || sel >= g_queues.size())
			sel = 0;
		listQueues->setCurrentRow(sel);
		queueItemActivated();
	}
	
	updateUi();
}

MainWindow::~MainWindow()
{
	saveWindowState();
	
	delete m_modelTransfers;
}

void MainWindow::setupUi()
{
	Ui_MainWindow::setupUi(this);
	
	m_dropBox = new DropBox(this);
	
	m_modelTransfers = new TransfersModel(this);
	treeTransfers->setModel(m_modelTransfers);
	treeTransfers->setItemDelegate(new ProgressDelegate(treeTransfers));
	
	m_trayIcon.setIcon(QIcon(":/fatrat/fatrat.png"));
	showTrayIcon();
	
	statusbar->addWidget(&m_labelStatus);
	
	statusbar->addPermanentWidget(new SpeedLimitWidget(statusbar));
	
	m_graph = new SpeedGraph(this);
	tabMain->insertTab(2, m_graph, QIcon(QString::fromUtf8(":/menu/network.png")), tr("Speed graph"));
	m_log = new LogManager(this, textTransferLog, textGlobalLog);
	
#ifdef WITH_DOCUMENTATION
	QAction* action;
	action = new QAction(QIcon(":/menu/about.png"), tr("Help"), menuHelp);
	menuHelp->insertAction(actionAboutQt, action);
	menuHelp->insertSeparator(actionAboutQt);
	connect(action, SIGNAL(triggered()), this, SLOT(showHelp()));
#endif
	
	connectActions();
}

void MainWindow::connectActions()
{
	connect(actionAbout, SIGNAL(triggered()), this, SLOT(about()));
	connect(actionNewQueue, SIGNAL(triggered()), this, SLOT(newQueue()));
	connect(actionDeleteQueue, SIGNAL(triggered()), this, SLOT(deleteQueue()));
	connect(actionQueueProperties, SIGNAL(triggered()), this, SLOT(queueItemProperties()));
	
	connect(listQueues, SIGNAL(itemSelectionChanged()), this, SLOT(queueItemActivated()));
	connect(listQueues, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(queueItemProperties()));
	connect(listQueues, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(queueItemContext(const QPoint&)));
	
	connect(treeTransfers, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(transferItemDoubleClicked(const QModelIndex&)));
	connect(treeTransfers, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(transferItemContext(const QPoint&)));
	
	QItemSelectionModel* model = treeTransfers->selectionModel();
	connect(model, SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)), this, SLOT(updateUi()));
	
	connect(actionNewTransfer, SIGNAL(triggered()), this, SLOT(addTransfer()));
	connect(actionDeleteTransfer, SIGNAL(triggered()), this, SLOT(deleteTransfer()));
	connect(actionDeleteTransferData, SIGNAL(triggered()), this, SLOT(deleteTransferData()));
	connect(actionRemoveCompleted, SIGNAL(triggered()), this, SLOT(removeCompleted()));
	
	connect(actionResume, SIGNAL(triggered()), this, SLOT(resumeTransfer()));
	connect(actionForcedResume, SIGNAL(triggered()), this, SLOT(forcedResumeTransfer()));
	connect(actionPause, SIGNAL(triggered()), this, SLOT(pauseTransfer()));
	
	connect(actionTop, SIGNAL(triggered()), this, SLOT(moveToTop()));
	connect(actionUp, SIGNAL(triggered()), this, SLOT(moveUp()));
	connect(actionDown, SIGNAL(triggered()), this, SLOT(moveDown()));
	connect(actionBottom, SIGNAL(triggered()), this, SLOT(moveToBottom()));
	connect(actionResumeAll, SIGNAL(triggered()), this, SLOT(resumeAll()));
	connect(actionStopAll, SIGNAL(triggered()), this, SLOT(stopAll()));
	
	connect(actionDropBox, SIGNAL(toggled(bool)), m_dropBox, SLOT(setVisible(bool)));
	connect(actionInfoBar, SIGNAL(toggled(bool)), this, SLOT(toggleInfoBar(bool)));
	connect(actionProperties, SIGNAL(triggered()), this, SLOT(transferOptions()));
	connect(actionDisplay, SIGNAL(toggled(bool)), this, SLOT(setVisible(bool)));
	connect(actionHideAllInfoBars, SIGNAL(triggered()),this, SLOT(hideAllInfoBars()));
	
	connect(actionOpenFile, SIGNAL(triggered()), this, SLOT(transferOpenFile()));
	connect(actionOpenDirectory, SIGNAL(triggered()), this, SLOT(transferOpenDirectory()));
	
	connect(actionComputeHash, SIGNAL(triggered()), this, SLOT(computeHash()));
	
	connect(pushGenericOptions, SIGNAL(clicked()), this, SLOT(transferOptions()));
	connect(tabMain, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));
	
	connect(actionQuit, SIGNAL(triggered()), qApp, SLOT(quit()));
	connect(actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
	connect(&m_trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
	connect(actionSettings, SIGNAL(triggered()), this, SLOT(showSettings()));
	
	connect(TransferNotifier::instance(), SIGNAL(stateChanged(Transfer*,Transfer::State,Transfer::State)), this, SLOT(downloadStateChanged(Transfer*,Transfer::State,Transfer::State)));
}

void MainWindow::restoreWindowState(bool bStartHidden)
{
	QHeaderView* hdr = treeTransfers->header();
	QVariant state = g_settings->value("state/mainheaders");
	
	if(state.isNull())
	{
		hdr->resizeSection(0, 300);
		hdr->resizeSection(3, 160);
	}
	else
		hdr->restoreState(state.toByteArray());
	
	state = g_settings->value("state/mainsplitter");
	if(state.isNull())
		splitterQueues->setSizes(QList<int>() << 80 << 600);
	else
		splitterQueues->restoreState(state.toByteArray());
	
	state = g_settings->value("state/statssplitter");
	if(!state.isNull())
		splitterStats->restoreState(state.toByteArray());
	
	connect(hdr, SIGNAL(sectionResized(int,int,int)), this, SLOT(saveWindowState()));
	connect(splitterQueues, SIGNAL(splitterMoved(int,int)), this, SLOT(saveWindowState()));
	
	if(!bStartHidden)
	{
		QPoint pos = g_settings->value("state/mainwindow_pos").toPoint();
		QSize size = g_settings->value("state/mainwindow_size").toSize();
		
		if(size.isEmpty())
		{
			qDebug() << "Maximizing the main window";
			showMaximized();
		}
		else
		{
			QWidget::move(pos);
			resize(size);
			show();
		}
	}
	else
		actionDisplay->setChecked(false);
}

void MainWindow::saveWindowState()
{	
	qDebug() << "saveWindowState()";
	
	g_settings->setValue("state/mainheaders", treeTransfers->header()->saveState());
	g_settings->setValue("state/mainsplitter", splitterQueues->saveState());
	g_settings->setValue("state/statssplitter", splitterStats->saveState());
	g_settings->setValue("state/mainwindow", saveGeometry());
	g_settings->setValue("state/selqueue", getSelectedQueue());
	
	g_settings->setValue("state/mainwindow_pos", pos());
	g_settings->setValue("state/mainwindow_size", size());
	
	g_settings->sync();
}

void MainWindow::about()
{
	AboutDlg(this).exec();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
	if(reason == QSystemTrayIcon::Trigger)
	{
		if(actionDisplay->isChecked() && !this->isActiveWindow())
		{
			activateWindow();
			raise();
		}
		else
			actionDisplay->toggle();
	}
	else if(reason == QSystemTrayIcon::Context)
	{
		QMenu menu(this);
		
		menu.addAction(actionDisplay);
		menu.addAction(actionDropBox);
		menu.addSeparator();
		menu.addAction(actionHideAllInfoBars);
		menu.addSeparator();
		menu.addAction(actionQuit);
		
		menu.exec(QCursor::pos());
	}
	else if(reason == QSystemTrayIcon::MiddleClick)
	{
		actionDropBox->toggle();
	}
}

void MainWindow::hideAllInfoBars()
{
	InfoBar::removeAll();
}

void MainWindow::hideEvent(QHideEvent* event)
{
	if(isMinimized() && m_trayIcon.isVisible() && g_settings->value("hideminimize", getSettingsDefault("hideminimize")).toBool())
	{
		actionDisplay->setChecked(false);
		hide();
	}
	else
		QWidget::hideEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	if(m_trayIcon.isVisible() && g_settings->value("hideclose", getSettingsDefault("hideclose")).toBool())
	{
		actionDisplay->setChecked(false);
		hide();
	}
	else
	{
		m_trayIcon.hide();
		QWidget::closeEvent(event);
		qApp->quit();
	}
}

void MainWindow::updateUi()
{
	QList<int> sel = getSelection();
	Queue* q = getCurrentQueue();
	
	// queue view
	if(q != 0)
	{
		actionDeleteQueue->setEnabled(true);
		actionQueueProperties->setEnabled(true);
		actionNewTransfer->setEnabled(true);
		actionStopAll->setEnabled(true);
		actionResumeAll->setEnabled(true);
	}
	else
	{
		actionDeleteQueue->setEnabled(false);
		actionQueueProperties->setEnabled(false);
		actionNewTransfer->setEnabled(false);
		actionStopAll->setEnabled(false);
		actionResumeAll->setEnabled(false);
	}
	
	// transfer view
	Transfer* d = 0;
	int currentTab = tabMain->currentIndex();
	
	if(!sel.empty())
	{
		int rcount = m_modelTransfers->rowCount();
		bool bSingle = sel.size() == 1;
		
		if(!bSingle && (currentTab == 1 || currentTab == 2))
		{
			tabMain->setCurrentIndex(0);
			currentTab = 0;
		}
		
		tabMain->setTabEnabled(1, bSingle);	// transfer details
		tabMain->setTabEnabled(2, bSingle);	// transfer graph
		
		actionOpenFile->setEnabled(bSingle);
		actionOpenDirectory->setEnabled(bSingle);
		
		if(bSingle)
		{
			actionTop->setEnabled(sel[0]);
			actionBottom->setEnabled(sel[0] < rcount-1);
			
			q->lock();
			
			d = q->at(sel[0]);
			actionInfoBar->setChecked(InfoBar::getInfoBar(d) != 0);
			
			actionForcedResume->setEnabled(d->statePossible(Transfer::ForcedActive));
			actionResume->setEnabled(d->statePossible(Transfer::Active));
			actionPause->setEnabled(d->statePossible(Transfer::Paused));
			
			m_graph->setRenderSource(d);
			m_log->setLogSource(d);
			
			q->unlock();
		}
		else
		{
			actionForcedResume->setEnabled(true);
			actionResume->setEnabled(true);
			actionPause->setEnabled(true);
			
			actionTop->setEnabled(true);
			actionUp->setEnabled(false);
			actionDown->setEnabled(false);
			actionBottom->setEnabled(true);
		}
		
		actionUp->setEnabled(true);
		actionDown->setEnabled(true);
		
		actionDeleteTransfer->setEnabled(true);
		actionDeleteTransferData->setEnabled(true);
		
		actionInfoBar->setEnabled(bSingle);
		actionProperties->setEnabled(bSingle);
	}
	else
	{
		if(currentTab == 1 || currentTab == 2)
		{
			tabMain->setCurrentIndex(0);
			currentTab = 0;
		}
		
		tabMain->setTabEnabled(1,false); // transfer details
		tabMain->setTabEnabled(2,false); // transfer graph
		
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
		
		m_graph->setRenderSource(0);
		m_log->setLogSource(0);
	}
	
	if(d != m_lastTransfer)
	{
		m_lastTransfer = d;
		transferItemActivated();
	}
	
	actionNewTransfer->setEnabled(q != 0);
	actionRemoveCompleted->setEnabled(q != 0);
	
	if(q != 0)
		doneQueue(q,true,false);
	
	m_modelTransfers->refresh();
	if(currentTab == 1)
		refreshDetailsTab();
}

void MainWindow::refreshQueues()
{
	g_queuesLock.lockForRead();
	
	int i;
	for(i=0;i<g_queues.size();i++)
	{
		QListWidgetItem* item;
		
		if(i>=listQueues->count())
		{
			item = new QListWidgetItem(g_queues[i]->name(), listQueues);
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled);
			listQueues->addItem(item);
		}
		else
		{
			item = listQueues->item(i);
			item->setText(g_queues[i]->name());
		}
		
		item->setData(Qt::UserRole, qVariantFromValue((void*) g_queues[i]));
	}
	
	while(i<listQueues->count())
		qDebug() << "Removing item" << i << listQueues->takeItem(i);
	
	int upt = 0, downt = 0;
	int upq = 0, downq = 0;
	int cur = getSelectedQueue();
	
	for(int j=0;j<g_queues.size();j++)
	{
		Queue* q = g_queues[j];
		q->lock();
		
		for(int i=0;i<q->size();i++)
		{
			int up,down;
			q->at(i)->speeds(down,up);
			downt += down;
			upt += up;
			
			if(j == cur)
			{
				downq += down;
				upq += up;
			}
		}
		
		q->unlock();
	}
	
	g_queuesLock.unlock();
	
	m_labelStatus.setText( QString(tr("Queue's speed: %1 down, %2 up")).arg(formatSize(downq,true)).arg(formatSize(upq,true)) );
}

void MainWindow::newQueue()
{
	QueueDlg dlg(this);
	
	if(dlg.exec() == QDialog::Accepted)
	{
		Queue* q = new Queue;
		
		q->setName(dlg.m_strName);
		q->setSpeedLimits(dlg.m_nDownLimit,dlg.m_nUpLimit);
		if(dlg.m_bLimit)
			q->setTransferLimits(dlg.m_nDownTransfersLimit,dlg.m_nUpTransfersLimit);
		else
			q->setTransferLimits(-1, -1);
		q->setUpAsDown(dlg.m_bUpAsDown);
		
		g_queuesLock.lockForWrite();
		g_queues << q;
		g_queuesLock.unlock();
		
		Queue::saveQueues();
		refreshQueues();
	}
}

void MainWindow::deleteQueue()
{
	int queue;
	
	queue = getSelectedQueue();
	
	if(g_queues.empty() || queue < 0)
		return;
	
	if(QMessageBox::warning(this, tr("Delete queue"),
	   tr("Do you really want to delete the active queue?"), QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
	{
		g_queuesLock.lockForWrite();
		g_queues.removeAt(queue);
		g_queuesLock.unlock();
		
		Queue::saveQueues();
		refreshQueues();
	}
}

void MainWindow::queueItemActivated()
{
	updateUi();
	treeTransfers->selectionModel()->clearSelection();
	m_modelTransfers->setQueue(getSelectedQueue());
	
	if(m_pDetailsDisplay)
		m_pDetailsDisplay->deleteLater();
	else
		displayDestroyed();
}

void MainWindow::queueItemProperties()
{
	QueueDlg dlg(this);
	
	Queue* q = getCurrentQueue(false);
	
	dlg.m_strName = q->name();
	dlg.m_bUpAsDown = q->upAsDown();
	q->speedLimits(dlg.m_nDownLimit, dlg.m_nUpLimit);
	q->transferLimits(dlg.m_nDownTransfersLimit,dlg.m_nUpTransfersLimit);
	
	if(dlg.m_nDownTransfersLimit < 0 || dlg.m_nUpTransfersLimit < 0)
	{
		dlg.m_nDownTransfersLimit = dlg.m_nUpTransfersLimit = 1;
		dlg.m_bLimit = false;
	}
	else
		dlg.m_bLimit = true;
	
	if(dlg.exec() == QDialog::Accepted)
	{
		q->setName(dlg.m_strName);
		q->setSpeedLimits(dlg.m_nDownLimit,dlg.m_nUpLimit);
		if(dlg.m_bLimit)
			q->setTransferLimits(dlg.m_nDownTransfersLimit,dlg.m_nUpTransfersLimit);
		else
			q->setTransferLimits();
		q->setUpAsDown(dlg.m_bUpAsDown);
		listQueues->currentItem()->setText(dlg.m_strName);
		
		Queue::saveQueues();
	}
	
	doneQueue(q, false);
}

void MainWindow::queueItemContext(const QPoint&)
{
	if(getSelectedQueue() != -1)
	{
		QMenu menu(listQueues);
		
		menu.addAction(actionResumeAll);
		menu.addAction(actionStopAll);
		menu.addSeparator();
		menu.addAction(actionQueueProperties);
		
		updateUi();
		menu.exec(QCursor::pos());
	}
}

void MainWindow::transferItemActivated()
{
	//updateUi();
	
	if(m_pDetailsDisplay)
		m_pDetailsDisplay->deleteLater();
	else
		displayDestroyed();
}

void MainWindow::displayDestroyed()
{
	Queue* q = getCurrentQueue();
	//QModelIndex i = treeTransfers->currentIndex();
	QList<int> sel = getSelection();
	Transfer* d = 0;
	
	if(q != 0 && sel.size() == 1)
		d = q->at(sel[0]);
	
	if(QWidget* w = stackedDetails->currentWidget())
	{
		//disconnect(w, SIGNAL(destroyed()), this, SLOT(displayDestroyed()));
		stackedDetails->removeWidget(w);
		delete w;
	}
	m_pDetailsDisplay = 0;
	
	if(d != 0)
	{
		QWidget* widgetDisplay = new QWidget(stackedDetails);
		m_pDetailsDisplay = d->createDetailsWidget(widgetDisplay);
		if(m_pDetailsDisplay)
		{
			stackedDetails->insertWidget(0,widgetDisplay);
			stackedDetails->setCurrentIndex(0);
			
			connect(m_pDetailsDisplay, SIGNAL(destroyed()), this, SLOT(displayDestroyed()));
		}
		else
			delete widgetDisplay;
	}
	
	doneQueue(q);
}

void MainWindow::transferItemDoubleClicked(const QModelIndex&)
{
	int op = g_settings->value("transfer_dblclk", getSettingsDefault("transfer_dblclk")).toInt();
	
	switch(op)
	{
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

void MainWindow::move(int i)
{
	Queue* q = getCurrentQueue(false);
	QList<int> sel = getSelection();
	
	if(!q) return;
	
	if(!sel.empty())
	{
		QItemSelectionModel* model = treeTransfers->selectionModel();
		
		int size = sel.size();
		model->blockSignals(true);
		model->clearSelection();
		
		switch(i)
		{
			case 0:
			{	
				for(int j=0;j<size;j++)
				{
					q->moveToTop(sel[size-j-1]+j);
					model->select(m_modelTransfers->index(j), QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
				break;
			}
			case 1:
				for(int i=0;i<size;i++)
				{
					int newpos;
					
					newpos = q->moveUp(sel[i]);
					model->select(m_modelTransfers->index(newpos), QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
				break;
			case 2:
				for(int i=size-1;i>=0;i--)
				{
					int newpos;
					
					newpos = q->moveDown(sel[i]);
					model->select(m_modelTransfers->index(newpos), QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
				break;
			case 3:
			{
				int qsize = q->size();
				QItemSelectionModel* model = treeTransfers->selectionModel();
				
				for(int i=0;i<size;i++)
				{
					q->moveToBottom(sel[i]-i);
					model->select(m_modelTransfers->index(qsize-i-1), QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
				break;
			}
		}
		
		model->blockSignals(false);
	}
	
	doneQueue(q,false);
	Queue::saveQueues();
}

void MainWindow::moveToTop()
{
	move(0);
}

void MainWindow::moveUp()
{
	move(1);
}

void MainWindow::moveDown()
{
	move(2);
}

void MainWindow::moveToBottom()
{
	move(3);
}

void MainWindow::addTransfer(QString uri, Transfer::Mode mode, QString className, int qSel)
{
	NewTransferDlg dlg(this);
	Queue* queue = 0;
	QList<Transfer*> listTransfers;
	
	if(qSel < 0)
	{
		qSel = getSelectedQueue();
		if(qSel < 0)
			return;
	}
	
	dlg.setWindowTitle(tr("New transfer"));
	dlg.m_nQueue = qSel;
	dlg.m_strURIs = uri;
	
	if(!uri.isEmpty() && className.isEmpty())
	{
		QStringList l = uri.split('\n', QString::SkipEmptyParts);
		Transfer::BestEngine eng = Transfer::bestEngine(l[0], mode);
		
		if(eng.type != Transfer::ModeInvalid)
			dlg.m_mode = eng.type;
	}
	else
	{
		dlg.m_mode = mode;
		dlg.m_strClass = className;
	}
	
	if(dlg.exec() != QDialog::Accepted)
		return;
	
	try
	{
		QStringList uris;
		int sep = g_settings->value("link_separator", getSettingsDefault("link_separator")).toInt();
		
		if(!sep)
			uris = dlg.m_strURIs.split('\n', QString::SkipEmptyParts);
		else
			uris = dlg.m_strURIs.split(QRegExp("\\s+"), QString::SkipEmptyParts);
		
		if(uris.isEmpty())
			return;

		for(int i=0;i<uris.size();i++)
			uris[i] = uris[i].trimmed();
		
		if(dlg.m_nClass == -1)
		{
			// autodetection
			Transfer::BestEngine eng;
			
			if(dlg.m_mode == Transfer::Download)
				eng = Transfer::bestEngine(uris[0], Transfer::Download);
			else
				eng = Transfer::bestEngine(dlg.m_strDestination, Transfer::Upload);
			
			if(eng.nClass < 0)
			{
				QMessageBox::critical(this, tr("Error"), tr("Couldn't autodetect transfer type."));
				return;
			}
			else
				dlg.m_nClass = eng.nClass;
		}
		
		queue = getQueue(dlg.m_nQueue, false);
		
		if(!queue)
			throw RuntimeException(tr("Internal error."));
		
		for(int i=0;i<uris.size();i++)
		{
			Transfer* d = Transfer::createInstance(dlg.m_mode, dlg.m_nClass);
			
			if(d == 0)
				throw RuntimeException(tr("Failed to create a class instance."));
			
			listTransfers << d;
			
			QString source, destination;
			
			source = uris[i].trimmed();
			destination = dlg.m_strDestination;
			
			if(!dlg.m_auth.strUser.isEmpty())
			{
				QString& obj = (dlg.m_mode == Transfer::Download) ? source : destination;
				
				QUrl url = obj;
				if(url.userInfo().isEmpty())
				{
					url.setUserName(dlg.m_auth.strUser);
					url.setPassword(dlg.m_auth.strPassword);
				}
				obj = url.toString();
			}
			
			d->init(source, destination);
			d->setUserSpeedLimits(dlg.m_nDownLimit,dlg.m_nUpLimit);
		}
		
		// show the transfer details dialog
		if(dlg.m_bDetails)
		{
			// show a typical transfer propeties dialog
			if(listTransfers.size() == 1)
			{
				WidgetHostDlg dlg(this);
				dlg.setWindowTitle(tr("Transfer details"));
				
				if(WidgetHostChild* q = listTransfers[0]->createOptionsWidget(dlg.getChildHost()))
				{
					dlg.addChild(q);
					
					if(dlg.exec() != QDialog::Accepted)
						throw RuntimeException();
				}
			}
			else // show a dialog designed for multiple
			{
				if(!Transfer::runProperties(this, dlg.m_mode, dlg.m_nClass, listTransfers))
					throw RuntimeException();
			}
		}
		
		if(!dlg.m_bPaused)
		{
			foreach(Transfer* d, listTransfers)
				d->setState(Transfer::Waiting);
		}
		
		queue->add(listTransfers);
	}
	catch(const RuntimeException& e)
	{
		qDeleteAll(listTransfers);
		if(!e.what().isEmpty())
			QMessageBox::critical(this, tr("Error"), e.what());
	}
	
	if(queue != 0)
		doneQueue(queue,false);
	
	Queue::saveQueues();
}

void MainWindow::deleteTransfer()
{
	Queue* q = getCurrentQueue(false);
	QList<int> sel = getSelection();
	
	if(!q) return;
	
	if(!sel.empty())
	{
		if(QMessageBox::warning(this, tr("Delete transfers"),
			tr("Do you really want to delete selected transfers?"),
			QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
		{
			treeTransfers->selectionModel()->clearSelection();
			
			q->lockW();
			for(int i=0;i<sel.size();i++)
				q->remove(sel[i]-i, true);
			q->unlock();
			Queue::saveQueues();
		}
	}
	
	doneQueue(q,false);
}

void MainWindow::deleteTransferData()
{
	Queue* q = getCurrentQueue(false);
	QList<int> sel = getSelection();
	
	if(!q) return;
	
	if(!sel.empty())
	{
		if(QMessageBox::warning(this, tr("Delete transfers"),
			tr("Do you really want to delete selected transfers <b>including the data</b>?"),
			QMessageBox::Yes|QMessageBox::No) == QMessageBox::Yes)
		{
			treeTransfers->selectionModel()->clearSelection();
			
			q->lockW();
			//bool bOK = true;
			
			for(int i=0;i<sel.size();i++)
				/*bOK &=*/ q->removeWithData(sel[i]-i, true);
			q->unlock();
			Queue::saveQueues();
			
			/*if(!bOK)
			{
				QMessageBox::warning(this, tr("Delete transfers"),
					tr("FatRat failed to remove some files, check your permissions."));
			}*/
		}
	}
	
	doneQueue(q,false);
}

void MainWindow::resumeTransfer()
{
	Queue* q = getCurrentQueue();
	QList<int> sel = getSelection();
	
	if(!q) return;
	
	foreach(int i,sel)
	{
		Transfer* d = q->at(i);
		Transfer::State state = d->state();
		
		if(state == Transfer::ForcedActive)
			d->setState(Transfer::Active);
		else if(state != Transfer::Active)
			d->setState(Transfer::Waiting);
	}
	
	doneQueue(q);
}

void MainWindow::forcedResumeTransfer()
{
	setTransfer(Transfer::ForcedActive);
}

void MainWindow::pauseTransfer()
{
	setTransfer(Transfer::Paused);
}

void MainWindow::setTransfer(Transfer::State state)
{
	Queue* q = getCurrentQueue();
	QList<int> sel = getSelection();
	
	if(!q) return;
	
	foreach(int i,sel)
	{
		Transfer* d = q->at(i);
		if(d->state() != Transfer::Active || state != Transfer::Waiting)
			d->setState(state);
	}
	
	doneQueue(q);
}

void MainWindow::transferOptions()
{
	WidgetHostDlg dlg(this);
	
	Queue* q = getCurrentQueue();
	Transfer* d;
	QModelIndex ctrans = treeTransfers->currentIndex();
	
	if(!q) return;
	
	d = q->at(ctrans.row());
	
	if(d != 0)
	{
		QWidget *widgetDetails;
		GenericOptsForm* wgt = new GenericOptsForm(dlg.getNextChildHost(tr("Generic options")));
		
		widgetDetails = dlg.getNextChildHost(tr("Details"));
		CommentForm* comment = new CommentForm (dlg.getNextChildHost(tr("Comment")), d);
		AutoActionForm* aaction = new AutoActionForm (dlg.getNextChildHost(tr("Actions")), d);
		
		wgt->m_mode = d->primaryMode();
		wgt->m_strURI = d->object();
		
		d->userSpeedLimits(wgt->m_nDownLimit, wgt->m_nUpLimit);
		wgt->m_nDownLimit /= 1024;
		wgt->m_nUpLimit /= 1024;
		
		dlg.setWindowTitle(tr("Transfer properties"));
		dlg.addChild(wgt);
		
		if(WidgetHostChild* c = d->createOptionsWidget(widgetDetails))
			dlg.addChild(c);
		else
			dlg.removeChildHost(widgetDetails);
		dlg.addChild(comment);
		dlg.addChild(aaction);
		
		if(dlg.exec() == QDialog::Accepted)
		{
			try
			{
				d->setObject(wgt->m_strURI);
			}
			catch(const std::exception& e)
			{
				QMessageBox::critical(this, tr("Error"), e.what());
			}
			d->setUserSpeedLimits(wgt->m_nDownLimit*1024,wgt->m_nUpLimit*1024);
			updateUi();
			Queue::saveQueues();
		}
	}
	doneQueue(q);
}

void MainWindow::refreshDetailsTab()
{
	Queue* q;
	Transfer* d;
	QList<int> sel = getSelection();
	QString progress;
	
	if((q = getCurrentQueue()) == 0)
	{
		tabMain->setCurrentIndex(0);
		return;
	}
	
	if(sel.size() != 1)
	{
		doneQueue(q, true, false);
		tabMain->setCurrentIndex(0);
		return;
	}
	
	d = q->at(sel[0]);
	
	lineName->setText(d->name());
	lineMessage->setText(d->message());
	lineDestination->setText(d->dataPath(false));
	
	if(d->total())
		progress = QString(tr("completed %1 from %2 (%3%)")).arg(formatSize(d->done())).arg(formatSize(d->total())).arg(100.0/d->total()*d->done(), 0, 'f', 1);
	else
		progress = QString(tr("completed %1, total size unknown")).arg(formatSize(d->done()));
	
	if(d->isActive())
	{
		int down,up;
		d->speeds(down,up);
		Transfer::Mode mode = d->primaryMode();
		QString speed;
		
		if(down)
			speed = QString("%1 kB/s down ").arg(double(down)/1024.f, 0, 'f', 1);
		if(up)
			speed += QString("%1 kB/s up").arg(double(up)/1024.f, 0, 'f', 1);
		
		if(d->total())
		{
			QString s;
			qulonglong totransfer = d->total() - d->done();
			
			if(down && mode == Transfer::Download)
				progress += QString(tr(", %1 left")).arg( formatTime(totransfer/down) );
			else if(up && mode == Transfer::Upload)
				progress += QString(tr(", %1 left")).arg( formatTime(totransfer/up) );
		}
		
		lineSpeed->setText( speed );
	}
	else
		lineSpeed->setText( QString() );
	
	lineProgress->setText( progress );
	lineRuntime->setText(formatTime(d->timeRunning()));
	
	doneQueue(q,true,false);
}

void MainWindow::currentTabChanged(int newTab)
{
	if(newTab == 1)
		refreshDetailsTab();
}

void MainWindow::removeCompleted()
{
	Queue* q;
	QString progress;
	
	if(getSelectedQueue() == -1)
	{
		tabMain->setCurrentIndex(0);
		return;
	}
	
	q = getCurrentQueue(false);
	q->lockW();
	
	for(int i=0;i<q->size();i++)
	{
		Transfer* d = q->at(i);
		if(d->state() == Transfer::Completed)
			q->remove(i--,true);
	}
	
	q->unlock();
	
	doneQueue(q, false, true);
}

void MainWindow::resumeAll()
{
	changeAll(true);
}

void MainWindow::stopAll()
{
	changeAll(false);
}

void MainWindow::changeAll(bool resume)
{
	Queue* q = getCurrentQueue();
	if(!q) return;
	
	for(int i=0;i<q->size();i++)
	{
		Transfer::State state = q->at(i)->state();
		
		if((state == Transfer::Paused || state == Transfer::Failed) && resume)
			q->at(i)->setState(Transfer::Waiting);
		else if(!resume && state != Transfer::Completed)
			q->at(i)->setState(Transfer::Paused);
	}
	
	doneQueue(q);
}

int MainWindow::getSelectedQueue()
{
	QModelIndexList list = listQueues->selectionModel()->selectedRows();
	
	if(list.isEmpty())
		return -1;
	else
		return list[0].row();
}

Queue* MainWindow::getQueue(int index, bool lock)
{
	g_queuesLock.lockForRead();
	
	if(index < 0 || index >= g_queues.size())
	{
		if(index != -1)
			qDebug() << "MainWindow::getQueue(): Invalid queue requested: " << index;
		g_queuesLock.unlock();
		return 0;
	}
	
	Queue* q = g_queues[index];
	if(lock)
		q->lock();
	return q;
}

Queue* MainWindow::getCurrentQueue(bool lock)
{
	return getQueue(getSelectedQueue(), lock);
}

void MainWindow::doneQueue(Queue* q, bool unlock, bool refresh)
{
	if(q != 0)
	{
		if(unlock)
			q->unlock();
		g_queuesLock.unlock();
		if(refresh)
			updateUi();
	}
}

void MainWindow::transferItemContext(const QPoint&)
{
	QList<int> sel = getSelection();
	if(!sel.isEmpty())
	{
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
		
		Queue* q = getCurrentQueue();
		if(q != 0)
		{
			if(sel.size() == 1)
			{
				Transfer* t = q->at(sel[0]);
				t->fillContextMenu(menu);
			}
			
			doneQueue(q, true, false);
		}
		
		menu.addSeparator();
		menu.addAction(actionInfoBar);
		menu.addSeparator();
		menu.addAction(actionProperties);
		
		updateUi();
		menu.exec(QCursor::pos());
	}
}

void MainWindow::toggleInfoBar(bool show)
{
	if(Queue* q = getCurrentQueue())
	{
		QModelIndex ctrans = treeTransfers->currentIndex();
		Transfer* d = q->at(ctrans.row());
		
		if(d != 0)
		{
			InfoBar* bar = InfoBar::getInfoBar(d);
			if(show && !bar)
				new InfoBar(this,d);
			else if(!show)
				delete bar;
		}
		
		doneQueue(q,true,false);
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasFormat("text/plain"))
		event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
	event->acceptProposedAction();
	addTransfer(event->mimeData()->text());
}

void MainWindow::showSettings()
{
	SettingsDlg(this).exec();
}

void MainWindow::showTrayIcon()
{
	m_trayIcon.setVisible(g_settings->value("trayicon", getSettingsDefault("trayicon")).toBool());
}

void MainWindow::downloadStateChanged(Transfer* d, Transfer::State prev, Transfer::State now)
{
	const bool popup = g_settings->value("showpopup", getSettingsDefault("showpopup")).toBool();
	const bool email = g_settings->value("sendemail", getSettingsDefault("sendemail")).toBool();
	
	if(!popup && !email)
		return;
	
	if((prev == Transfer::Active || prev == Transfer::ForcedActive) && (now == Transfer::Completed))
	{
		if(popup)
		{
			m_trayIcon.showMessage(tr("Transfer completed"),
					QString(tr("The transfer of \"%1\" has been completed.")).arg(d->name()),
					QSystemTrayIcon::Information, g_settings->value("popuptime", getSettingsDefault("popuptime")).toInt()*1000);
		}
		if(email)
		{
			QString from,to;
			QString message;
			
			from = g_settings->value("emailsender", getSettingsDefault("emailsender")).toString();
			to = g_settings->value("emailrcpt", getSettingsDefault("emailrcpt")).toString();
			
			message = QString("From: <%1>\r\nTo: <%2>\r\n"
					"Subject: FatRat transfer completed\r\n"
					"X-Mailer: FatRat/" VERSION "\r\n"
					"The transfer of \"%3\" has been completed.").arg(from).arg(to).arg(d->name());
			
			new SimpleEmail(g_settings->value("smtpserver",getSettingsDefault("smtpserver")).toString(),from,to,message);
		}
	}
}

void MainWindow::downloadModeChanged(Transfer* /*t*/, Transfer::State /*prev*/, Transfer::State /*now*/)
{
}

QList<int> MainWindow::getSelection()
{
	QModelIndexList list = treeTransfers->selectionModel()->selectedRows();
	QList<int> result;
	
	if(Queue* q = getCurrentQueue())
	{
		int size = q->size();
		
		doneQueue(q, true, false);
		
		foreach(QModelIndex in, list)
		{
			int row = in.row();
			
			if(row < size)
				result << row;
		}
		
		qSort(result);
	}
	
	return result;
}

void MainWindow::transferOpen(bool bOpenFile)
{
	QList<int> sel = getSelection();
	
	if(sel.size() != 1)
		return;
	
	if(Queue* q = getCurrentQueue())
	{
		QString path;
		Transfer* d = q->at(sel[0]);
		QString obj = d->object();
		
		path = d->dataPath(bOpenFile);
		
		doneQueue(q, true, false);
		
		QString command = QString("%1 \"%2\"")
				.arg(g_settings->value("fileexec", getSettingsDefault("fileexec")).toString())
				.arg(path);
		QProcess::startDetached(command);
	}
}

void MainWindow::transferOpenDirectory()
{
	transferOpen(false);
}

void MainWindow::transferOpenFile()
{
	transferOpen(true);
}

void MainWindow::computeHash()
{
	HashDlg(this).exec();
}

void MainWindow::reconfigure()
{
	showTrayIcon();
	if(m_dropBox != 0)
		m_dropBox->reconfigure();
}

void MainWindow::unhide()
{
	if(m_trayIcon.isVisible() && !actionDisplay->isChecked())
		actionDisplay->toggle();
}

void MainWindow::showHelp()
{
#ifdef WITH_DOCUMENTATION
	QWidget* w = new HelpBrowser;
	connect(w, SIGNAL(changeTabTitle(QString)), tabMain, SLOT(changeTabTitle(QString)));
	tabMain->setCurrentIndex( tabMain->addTab(w, QIcon(":/menu/about.png"), tr("Help")) );
#endif
}

