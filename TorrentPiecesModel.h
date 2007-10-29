#ifndef TORRENTPIECESMODEL_H
#define TORRENTPIECESMODEL_H
#include <QAbstractListModel>
#include <QItemDelegate>
#include <vector>
#include <QStringList>
#include <libtorrent/session.hpp>

class TorrentDownload;

class BlockDelegate : public QItemDelegate
{
Q_OBJECT
public:
	BlockDelegate(QObject* parent=0) : QItemDelegate(parent) {}
	QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const { return QSize(60,20); }
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TorrentPiecesModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentPiecesModel(QObject* parent, TorrentDownload* d);
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
	
	void refresh();
private:
	TorrentDownload* m_download;
	int m_nLastRowCount;
	QStringList m_columns;
protected:
	std::vector<libtorrent::partial_piece_info> m_pieces;
	friend class BlockDelegate;
};

#endif
