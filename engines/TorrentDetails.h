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
	std::vector<bool> m_vecPieces;
	TorrentPiecesModel* m_pPiecesModel;
	TorrentPeersModel* m_pPeersModel;
	TorrentFilesModel* m_pFilesModel;
	
	QList<int> m_selFiles;
	QMenu *m_pMenuFiles, *m_pMenuPeers;
};

#endif
