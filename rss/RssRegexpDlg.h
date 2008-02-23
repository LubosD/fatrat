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
public:
	QList<RssFeed> m_feeds;
	QString m_strFeed, m_strExpression, m_strQueue, m_strTarget;
	QString m_strTVSFrom, m_strTVSTo;
	RssRegexp::TVSType m_tvs;
	bool m_bTVSTrailers, m_bTVSRepacks, m_bTVSNoManuals;
	
	QString m_strFeedName;
};

#endif
