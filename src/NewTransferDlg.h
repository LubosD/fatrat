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
*/

#ifndef NEWTRANSFERDLG_H
#define NEWTRANSFERDLG_H
#include <QWidget>
#include "ui_NewTransferDlg.h"
#include "Transfer.h"
#include "Auth.h"
#include <QDir>

class NewTransferDlg : public QDialog, Ui_NewTransferDlg
{
Q_OBJECT
public:
	NewTransferDlg(QWidget* parent);
	int exec();
	void accepted();
	void load();
	void addLinks(QString links);
private:
	QString getDestination() const;
	void setDestination(QString p);
private slots:
	void browse();
	void browse2();
	void switchMode();
	void authData();
	
	void addTextFile();
	void addClipboard();
	
	void queueChanged(int now);
public:
	QString m_strURIs,m_strDestination,m_strClass;
	QStringList m_lastDirs;
	Auth m_auth;
	bool m_bDetails, m_bPaused, m_bNewTransfer;
	int m_nDownLimit, m_nUpLimit, m_nClass;
	Transfer::Mode m_mode;
	int m_nQueue;
};

#endif
