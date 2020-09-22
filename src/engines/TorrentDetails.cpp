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

#include "TorrentDetails.h"
#include "TorrentPiecesModel.h"
#include "TorrentPeersModel.h"
#include "TorrentFilesModel.h"
#include "Settings.h"
#include "fatrat.h"
#include <QHeaderView>
#include <QMenu>
#include <QProcess>
#include <QMessageBox>
#include <QSettings>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/peer_info.hpp>
#include <libtorrent/torrent_info.hpp>

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
	for (int i : m_selFiles)
		m_download->m_vecPriorities[i] = libtorrent::download_priority_t(p);
	m_download->m_handle.prioritize_files(m_download->m_vecPriorities);
}

void TorrentDetails::openFile()
{
	if(m_selFiles.size() != 1)
		return;
	
	int i = m_selFiles[0];
	
	QString relative = QString::fromStdString(m_download->m_info->files().file_path(i));
	QString path = m_download->dataPath(false);
	
	if(!path.endsWith('/'))
		path += '/';
	path += relative;

	QProcess::startDetached(getSettingsValue("fileexec").toString(), QStringList() << path);
}

void TorrentDetails::peerInfo()
{
	int row = treePeers->currentIndex().row();
	if(row != -1)
	{
		const libtorrent::peer_info& info = m_pPeersModel->m_peers[row];
		QMessageBox::information(treePeers, "FatRat", QString("Peak download rate: %1\nPeak upload rate: %2")
								.arg(formatSize(info.download_rate_peak, true)
								.arg(formatSize(info.upload_rate_peak, true))));
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
	if(m_download && m_download->m_handle.is_valid() && m_download->m_info)
	{
		m_bFilled = true;
		
		boost::optional<time_t> time = m_download->m_info->creation_date();
		if(time)
		{
			std::string created = boost::posix_time::to_simple_string(boost::posix_time::from_time_t(time.get()));
			lineCreationDate->setText(created.c_str());
		}
		linePieceLength->setText( QString("%1 kB").arg(m_download->m_info->piece_length()/1024.f) );
		
		QString comment = m_download->m_info->comment().c_str();
		comment.replace('\n', "<br>");
		textComment->setHtml(comment);
		
		lineCreator->setText(m_download->m_info->creator().c_str());
		linePrivate->setText( m_download->m_info->priv() ? tr("yes") : tr("no"));
		
		QString magnet = QString::fromStdString(libtorrent::make_magnet_uri(m_download->m_handle));
		lineMagnet->setText(magnet);
	}
}

void TorrentDetails::refresh()
{
	if(m_download && m_download->m_handle.is_valid() && m_download->m_info)
	{
		if(!m_bFilled)
			fill();
		
		// GENERAL
		int next = std::chrono::duration_cast<std::chrono::seconds>(m_download->m_status.next_announce).count();
		
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
		
		lineTracker->setText(tr("%1 (refresh in %2:%3:%4)")
				.arg(m_download->m_status.current_tracker.c_str())
				.arg(next / 3600).arg(next / 60,2,10,QChar('0')).arg(next % 60,2,10,QChar('0')));
		
		libtorrent::bitfield pieces = m_download->m_status.pieces;
		
		if(pieces.empty() && m_download->m_info->total_size() == m_download->m_status.total_done)
		{
			pieces.resize(m_download->m_info->num_pieces());
			pieces.set_all();
		}
		
		if(!pieces.empty() && !bitfieldsEqual(m_vecPieces, pieces))
		{
			widgetCompletition->generate(pieces);
			m_vecPieces = pieces;
			
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

bool TorrentDetails::bitfieldsEqual(const libtorrent::bitfield& b1, const libtorrent::bitfield& b2)
{
	const char* pb1, *pb2;
	
	pb1 = b1.data();
	pb2 = b2.data();
	
	if(!pb1 && !pb2)
		return true;
	else if((pb1 && !pb2) || (!pb1 && pb2))
		return false;
	else if(b1.size() != b2.size())
		return false;
	else
		return memcmp(pb1, pb2, b1.size()/2) == 0;
}

