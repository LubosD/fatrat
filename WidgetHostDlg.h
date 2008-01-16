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
