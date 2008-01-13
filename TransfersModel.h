#ifndef _TRANSFERSMODEL_H
#define _TRANSFERSMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QTreeView>
#include <QItemDelegate>
#include "Queue.h"
#include <QPixmap>

class ProgressDelegate : public QItemDelegate
{
public:
	ProgressDelegate(QObject* parent=0) : QItemDelegate(parent) {}
	QSize sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const { return QSize(60,25); }
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

class TransfersModel : public QAbstractListModel
{
Q_OBJECT
public:
	TransfersModel(QObject* parent);
	~TransfersModel();
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex parent(const QModelIndex &index) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	QVariant data(const QModelIndex &index, int role) const;
	bool hasChildren ( const QModelIndex & parent = QModelIndex() ) const;
	Qt::DropActions supportedDragActions() const;
	QStringList mimeTypes() const { return QStringList("application/x-fatrat-transfer"); }
	QMimeData* mimeData(const QModelIndexList &indexes) const;
	
	void setQueue(int q);
	void refresh();
protected:
	int m_queue;
private:
	int m_nLastRowCount;
	QIcon* m_states[12];
	
	struct RowData
	{
		Transfer::State state;
		QString name, speed, timeLeft, message, progress, size;
		Transfer::Mode mode, primaryMode;
		
		inline bool operator!=(const RowData& d2)
		{
#define COMP(n) n != d2.n
			return COMP(state) || COMP(name) || COMP(speed) || COMP(timeLeft) || COMP(message) ||
					COMP(progress) || COMP(size) || COMP(mode) || COMP(primaryMode);
#undef COMP
		}
	};
	
	static RowData createDataSet(Transfer* t);
	
	QVector<RowData> m_lastData;
	
	friend class ProgressDelegate;
};

#endif
