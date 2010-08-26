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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef _TRANSFERSMODEL_H
#define _TRANSFERSMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QTreeView>
#include <QItemDelegate>
#include "Queue.h"
#include <QPixmap>
#include <QMap>

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
	int remapIndex(int index);
protected:
	int m_queue;
private:
	int m_nLastRowCount;
	QIcon* m_states[12];
	QMap<int,int> m_filterMapping;
	
	struct RowData
	{
		Transfer::State state;
		QString name, speedDown, speedUp, timeLeft, message, progress, size;
		Transfer::Mode mode, primaryMode;
		float fProgress;
		
		inline bool operator!=(const RowData& d2)
		{
#define COMP(n) n != d2.n
			return COMP(state) || COMP(name) || COMP(speedDown) || COMP(speedUp) || COMP(timeLeft) || COMP(message) ||
					COMP(progress) || COMP(size) || COMP(mode) || COMP(primaryMode);
#undef COMP
		}
	};
	
	static RowData createDataSet(Transfer* t);
	
	QVector<RowData> m_lastData;
	
	friend class ProgressDelegate;
};

#endif
