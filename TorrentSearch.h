#ifndef TORRENTSEARCH_H
#define TORRENTSEARCH_H
#include <QWidget>
#include <QList>
#include <QPair>
#include "ui_TorrentSearch.h"

class QHttp;
class QBuffer;

class TorrentSearch : public QWidget, Ui_TorrentSearch
{
Q_OBJECT
public:
	TorrentSearch();
	virtual ~TorrentSearch();
	static QWidget* create() { return new TorrentSearch; }
protected:
	struct RegExpParam
	{
		QString regexp;
		int field, match;
	};
	
	struct Engine
	{
		QString id, name;
		QString query, postData;
		
		// parsing blocks
		QString beginning, splitter, ending;
		
		// parsing items
		QList<QPair<QString, RegExpParam> > regexps;
		
		QHttp* http;
		QBuffer* buffer;
	};
	
	void updateUi();
	void loadEngines();
	void parseResults(Engine* e);
	
	static QList<QByteArray> splitArray(const QByteArray& src, QString sep);
signals:
	void changeTabTitle(QString newTitle);
public slots:
	void search();
	void searchDone(bool error);
	void itemDoubleClicked(QTreeWidgetItem* item);
	void setSearchFocus();
private:
	QList<Engine> m_engines;
	bool m_bSearching;
};

class SearchTreeWidgetItem : public QTreeWidgetItem
{
public:
	SearchTreeWidgetItem(QTreeWidget* parent) : QTreeWidgetItem(parent) {}
	
	void parseSize(QString in);
	
	QString m_strLink; // torrent download link
	qint64 m_nSize; // torrent's data size
private:
	bool operator<(const QTreeWidgetItem& other) const;
};

#endif
