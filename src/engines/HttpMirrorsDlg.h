/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef HTTPMIRRORSDLG_H
#define HTTPMIRRORSDLG_H
#include <QDialog>
#include <QMap>
#include <QSet>
#include <QThread>
#include "ui_HttpMirrorsDlg.h"

class HttpMirrorsDlg : public QDialog, Ui_HttpMirrorsDlg
{
Q_OBJECT
public:
	HttpMirrorsDlg(QWidget* parent);
	~HttpMirrorsDlg();
	void load(const QMap<QString, QStringList>& mirrors, QSet<QString> compatible);
	QMap<QString,QStringList> pickedUrls() const;
private:

	class ProbeThread : public QThread
	{
	public:
		ProbeThread(QMap<QString,QTreeWidgetItem*>& servers, QObject* parent);
		~ProbeThread();
		void stop() { m_bStop = true; }
		virtual void run();
	private:
		QMap<QString,QTreeWidgetItem*> m_servers;
		bool m_bStop;
	} *m_probeThread;

	class CSTreeWidgetItem : public QTreeWidgetItem
	{
	public:
		CSTreeWidgetItem(QTreeWidgetItem* parent);
		virtual bool operator<(const QTreeWidgetItem& other) const;
	};
};

#endif // HTTPMIRRORSDLG_H
