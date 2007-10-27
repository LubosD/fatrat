#ifndef TORRENTPEERSMODEL_H
#define TORRENTPEERSMODEL_H
#include <QAbstractListModel>
#include <QStringList>
#include <vector>
#include <libtorrent/session.hpp>

class TorrentDownload;

class TorrentPeersModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentPeersModel(QObject* parent, TorrentDownload* d);
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
	
	void refresh();
protected:
	std::vector<libtorrent::peer_info> m_peers;
private:
	TorrentDownload* m_download;
	int m_nLastRowCount;
	QStringList m_columns;
};

#endif
