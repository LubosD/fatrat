/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "TorrentDetails.h"
#include "TorrentPiecesModel.h"
#include "TorrentPeersModel.h"
#include "TorrentFilesModel.h"
#include <QHeaderView>
#include <QMenu>
#include <QProcess>
#include <QMessageBox>
#include <QSettings>
#include <boost/date_time/posix_time/posix_time.hpp>

extern QSettings* g_settings;

TorrentDetails::TorrentDetails(QWidget* me, TorrentDownload* obj)
	: m_download(obj), m_bFilled(false)
{
	connect(obj, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
	setupUi(me);
	TorrentDownload::m_worker->setDetailsObject(this);
	
	m_pPiecesModel = new TorrentPiecesModel(treePieces, obj);
	treePieces->setModel(m_pPiecesModel);
	treePieces->setItemDelegate(new BlockDelegate(treePieces));
	
	m_pPeersModel = new TorrentPeersModel(treePeers, obj);
	treePeers->setModel(m_pPeersModel);
	
	m_pFilesModel = new TorrentFilesModel(treeFiles, obj);
	treeFiles->setModel(m_pFilesModel);
	treeFiles->setItemDelegate(new TorrentProgressDelegate(treeFiles));
	
	QHeaderView* hdr = treePeers->header();
	hdr->resizeSection(1, 110);
	hdr->resizeSection(3, 50);
	hdr->resizeSection(4, 70);
	
	for(int i=5;i<9;i++)
		hdr->resizeSection(i, 70);
	
	hdr->resizeSection(9, 300);
	
	hdr = treeFiles->header();
	hdr->resizeSection(0, 500);
	
	QAction* act;
	QMenu* submenu;
	
	m_pMenuFiles = new QMenu(me);
	submenu = new QMenu(tr("Priority"), m_pMenuFiles);
	
	act = submenu->addAction( tr("Do not download") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority0()));
	act = submenu->addAction( tr("Normal") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority1()));
	act = submenu->addAction( tr("Increased") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority4()));
	act = submenu->addAction( tr("Maximum") );
	connect(act, SIGNAL(triggered()), this, SLOT(setPriority7()));
	
	m_pMenuFiles->addAction(actionOpenFile);
	m_pMenuFiles->addMenu(submenu);
	
	connect(treeFiles, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(fileContext(const QPoint&)));
	connect(treeFiles, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(openFile()));
	
	m_pMenuPeers = new QMenu(me);
	act = m_pMenuPeers->addAction( tr("Ban") );
	act = m_pMenuPeers->addAction( tr("Information") );
	connect(act, SIGNAL(triggered()), this, SLOT(peerInfo()));
	
	connect(treePeers, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(peerContext(const QPoint&)));
	connect(actionOpenFile, SIGNAL(triggered()), this, SLOT(openFile()));
	
	refresh();
}

TorrentDetails::~TorrentDetails()
{
}

void TorrentDetails::setPriority(int p)
{
	foreach(int i, m_selFiles)
		m_download->m_vecPriorities[i] = p;
	m_download->m_handle.prioritize_files(m_download->m_vecPriorities);
}

void TorrentDetails::openFile()
{
	if(m_selFiles.size() != 1)
		return;
	
	int i = m_selFiles[0];
	
	QString relative = m_download->m_info->file_at(i).path.string().c_str();
	QString path = m_download->dataPath(false) + relative;
	
	QString command = QString("%1 \"%2\"")
			.arg(g_settings->value("fileexec", getSettingsDefault("fileexec")).toString())
			.arg(path);
	QProcess::startDetached(command);
}

void TorrentDetails::peerInfo()
{
	int row = treePeers->currentIndex().row();
	if(row != -1)
	{
		const libtorrent::peer_info& info = m_pPeersModel->m_peers[row];
		QMessageBox::information(treePeers, "FatRat", QString("Load balancing: %1").arg(info.load_balancing));
	}
}

void TorrentDetails::fileContext(const QPoint&)
{
	if(m_download && m_download->m_handle.is_valid())
	{
		int numFiles = m_download->m_info->num_files();
		QModelIndexList list = treeFiles->selectionModel()->selectedRows();
		
		m_selFiles.clear();
		
		foreach(QModelIndex in, list)
		{
			int row = in.row();
			
			if(row < numFiles)
				m_selFiles << row;
		}
		
		actionOpenFile->setEnabled(list.size() == 1);
		m_pMenuFiles->exec(QCursor::pos());
	}
}

void TorrentDetails::peerContext(const QPoint&)
{
	//m_pMenuPeers->exec(QCursor::pos());
}

void TorrentDetails::destroy()
{
	m_download = 0;
}

void TorrentDetails::fill()
{
	if(m_download && m_download->m_handle.is_valid())
	{
		m_bFilled = true;
		
		boost::optional<boost::posix_time::ptime> time = m_download->m_info->creation_date();
		if(time)
		{
			std::string created = boost::posix_time::to_simple_string(time.get());
			lineCreationDate->setText(created.c_str());
		}
		linePieceLength->setText( QString("%1 kB").arg(m_download->m_info->piece_length()/1024.f) );
		
		QString comment = m_download->m_info->comment().c_str();
		comment.replace('\n', "<br>");
		textComment->setHtml(comment);
		
		lineCreator->setText(m_download->m_info->creator().c_str());
		linePrivate->setText( m_download->m_info->priv() ? tr("yes") : tr("no"));
		
		m_pFilesModel->fill();
	}
}

void TorrentDetails::refresh()
{
	if(m_download && m_download->m_handle.is_valid())
	{
		if(!m_bFilled)
			fill();
		
		// GENERAL
		boost::posix_time::time_duration& next = m_download->m_status.next_announce;
		boost::posix_time::time_duration& intv = m_download->m_status.announce_interval;
		
		// availability
		QPalette palette = QApplication::palette(lineAvailability);
		if(m_download->m_status.distributed_copies != -1)
		{
			if(m_download->m_status.distributed_copies < 1.0)
				palette.setColor(palette.Text, Qt::red);
			lineAvailability->setText(QString::number(m_download->m_status.distributed_copies));
		}
		else
			lineAvailability->setText("-");
		lineAvailability->setPalette(palette);
		
		lineTracker->setText(tr("%1 (refresh in %2:%3:%4, every %5:%6:%7)")
				.arg(m_download->m_status.current_tracker.c_str())
				.arg(next.hours()).arg(next.minutes(),2,10,QChar('0')).arg(next.seconds(),2,10,QChar('0'))
				.arg(intv.hours()).arg(intv.minutes(),2,10,QChar('0')).arg(intv.seconds(),2,10,QChar('0')));
		
		const std::vector<bool>* pieces = m_download->m_status.pieces;
		
		if(pieces != 0 && m_vecPieces != (*pieces))
		{
			widgetCompletition->generate(*pieces);
			m_vecPieces = *pieces;
			
			// FILES
			m_pFilesModel->refresh(&m_vecPieces);
		}
		
		std::vector<int> avail;
		m_download->m_handle.piece_availability(avail);
		widgetAvailability->generate(avail);
		
		// ratio
		qint64 d, u;
		QString ratio;
		
		d = m_download->totalDownload();
		u = m_download->totalUpload();
		
		if(!d)
			ratio = QString::fromUtf8("∞");
		else
			ratio = QString::number(double(u)/double(d));
		lineRatio->setText(ratio);
		
		lineTotalDownload->setText(formatSize(d));
		lineTotalUpload->setText(formatSize(u));
		
		// PIECES IN PROGRESS
		m_pPiecesModel->refresh();
		
		// CONNECTED PEERS
		m_pPeersModel->refresh();
	}
}
