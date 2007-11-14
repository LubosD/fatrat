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
		QString query;
		
		// parsing blocks
		QString beginning, splitter, ending;
		
		// parsing items
		QList<QPair<QString, RegExpParam> > regexps;
		
		QHttp* http;
		QBuffer* buffer;
	};
	
	void loadEngines();
	void parseResults(Engine* e);
	
	static QList<QByteArray> splitArray(const QByteArray& src, QString sep);
public slots:
	void search();
	void searchDone(bool error);
private:
	QList<Engine> m_engines;
	bool m_bSearching;
};

#endif
