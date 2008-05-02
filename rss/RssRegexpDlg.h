#ifndef RSSREGEXPDLG_H
#define RSSREGEXPDLG_H
#include <QDialog>
#include "ui_RssRegexpDlg.h"
#include "RssFetcher.h"

class RssRegexpDlg : public QDialog, Ui_RssRegexpDlg
{
Q_OBJECT
public:
	RssRegexpDlg(QWidget* parent);
	int exec();
protected slots:
	void browse();
	void test();
	void updateTVS();
	void linkClicked(const QString& link);
public:
	QList<RssFeed> m_feeds;
	
	QString m_strFeedName;
	RssRegexp m_regexp;
};

#endif
