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

#ifndef TORRENTDETAILS_H
#define TORRENTDETAILS_H

#include "TorrentDownload.h"
#include "ui_TorrentDetailsForm.h"

class TorrentPiecesModel;
class TorrentPeersModel;
class TorrentFilesModel;

class TorrentDetails : public QObject, Ui_TorrentDetailsForm
{
Q_OBJECT
public:
	TorrentDetails(QWidget* me, TorrentDownload* obj);
	virtual ~TorrentDetails();
	void fill(); // only constant data
	void setPriority(int p);
	static bool bitfieldsEqual(const libtorrent::bitfield& b1, const libtorrent::bitfield& b2);
public slots:
	void refresh();
	void destroy();
	void fileContext(const QPoint&);
	void peerContext(const QPoint&);
	
	void setPriority0() { setPriority(0); }
	void setPriority1() { setPriority(1); }
	void setPriority4() { setPriority(4); }
	void setPriority7() { setPriority(7); }
	void openFile();
	
	void peerInfo();
private:
	TorrentDownload* m_download;
	bool m_bFilled;
	libtorrent::bitfield m_vecPieces;
	TorrentPiecesModel* m_pPiecesModel;
	TorrentPeersModel* m_pPeersModel;
	TorrentFilesModel* m_pFilesModel;
	
	QList<int> m_selFiles;
	QMenu *m_pMenuFiles, *m_pMenuPeers;
};

#endif
