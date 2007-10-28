#ifndef TORRENTFILESMODEL_H
#define TORRENTFILESMODEL_H
#include <QAbstractListModel>
#include <QList>
#include <QVector>
#include <QStringList>
#include <QItemDelegate>
#include <vector>
#include <libtorrent/session.hpp>

class TorrentDownload;

class TorrentProgressDelegate : public QItemDelegate
{
public:
	TorrentProgressDelegate(QObject* parent = 0) : QItemDelegate(parent) {}
	QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const { return QSize(60,20); }
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TorrentFilesModel : public QAbstractListModel
{
Q_OBJECT
public:
	TorrentFilesModel(QObject* parent, TorrentDownload* d);
	
	QModelIndex index(int row, int column, const QModelIndex& parent) const;
	QModelIndex parent(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent) const;
	int columnCount(const QModelIndex&) const { return m_columns.size(); }
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren(const QModelIndex& parent) const;
	
	void fill();
	void refresh(const std::vector<bool>* pieces);
protected:
	QList<libtorrent::file_entry> m_files;
	QList<int> m_priorities;
	//QVector<float> m_progresses;
	std::vector<float> m_progresses;
	const std::vector<bool>* m_pieces;
private:
	TorrentDownload* m_download;
	QStringList m_columns;
	
	friend class TorrentProgressDelegate;
};

#endif
