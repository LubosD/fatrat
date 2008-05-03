#include "RssDownloadedDlg.h"
#include "RssFetcher.h"
#include "ui_RssEpisodeNameDlg.h"

RssDownloadedDlg::RssDownloadedDlg(QStringList* eps, const char* mask, QWidget* parent)
	: QDialog(parent), m_episodes(eps), m_mask(mask)
{
	setupUi(this);
	
	connect(pushAdd, SIGNAL(clicked()), this, SLOT(add()));
	connect(pushDelete, SIGNAL(clicked()), this, SLOT(remove()));
}

int RssDownloadedDlg::exec()
{
	int r;
	
	for(int i=0;i<m_episodes->size();i++)
		listEpisodes->addItem(m_episodes->at(i));
	
	if((r = QDialog::exec()) != QDialog::Accepted)
		return r;
	
	m_episodes->clear();
	for(int i=0;i<listEpisodes->count();i++)
		m_episodes->append(listEpisodes->item(i)->text());
	
	return r;
}

void RssDownloadedDlg::add()
{
	QDialog dlg(this);
	Ui_RssEpisodeNameDlg ui;
	
	ui.setupUi(&dlg);
	ui.lineEpisode->setInputMask(m_mask);
	if(dlg.exec() == QDialog::Accepted)
		listEpisodes->addItem(ui.lineEpisode->text());
}

void RssDownloadedDlg::remove()
{
	int row = listEpisodes->currentRow();
	if(row != -1)
		delete listEpisodes->takeItem(row);
}
