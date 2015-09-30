/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

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
