/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef _WIDGETHOSTDLG_H
#define _WIDGETHOSTDLG_H
#include "ui_WidgetHostDlg.h"
#include <QList>
#include <QTabWidget>
#include "WidgetHostChild.h"

class WidgetHostDlg : public QDialog, private Ui_WidgetHostDlg
{
Q_OBJECT
public:
	WidgetHostDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		m_tabs = new QTabWidget(stackedMain);
		stackedMain->insertWidget(1,m_tabs);
	}
	~WidgetHostDlg()
	{
		qDeleteAll(m_children);
	}
	
	QWidget* getChildHost()
	{
		return widgetGeneric;
	}
	void addChild(WidgetHostChild* child)
	{
		//setWindowTitle(child->windowTitle());
		m_children << child;
	}
	
	QWidget* getNextChildHost(QString label)
	{
		QWidget* sub = new QWidget(m_tabs);
		stackedMain->setCurrentIndex(1);
		m_tabs->addTab(sub,label);
		return sub;
	}
	void removeChildHost(QWidget* w)
	{
		m_tabs->removeTab(m_tabs->indexOf(w));
	}
	
	virtual void accept()
	{
		bool accepted = true, acc;
		
		foreach(WidgetHostChild* w,m_children)
		{
			acc = w->accept();
			accepted = accepted && acc;
			
			if(!accepted)
				break;
		}
		
		if(accepted)
		{
			foreach(WidgetHostChild* w,m_children)
			{
				w->accepted();
			}
			
			QDialog::accept();
		}
	}
	int exec()
	{
		foreach(WidgetHostChild* w,m_children)
		{
			w->load();
		}
		return QDialog::exec();
	}
private:
	QList<WidgetHostChild*> m_children;
	QTabWidget* m_tabs;
};

#endif
