#ifndef RSSDOWNLOADEDDLG_H
#define RSSDOWNLOADEDDLG_H
#include "ui_RssDownloadedDlg.h"
#include <QDialog>

class RssDownloadedDlg : public QDialog, Ui_RssDownloadedDlg
{
Q_OBJECT
public:
	RssDownloadedDlg(QStringList* eps, const char* mask, QWidget* parent);
	int exec();
public slots:
	void add();
	void remove();
private:
	QStringList* m_episodes;
	const char* m_mask;
};

#endif
