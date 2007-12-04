#ifndef MAINTAB_H
#define MAINTAB_H
#include <QMenu>
#include <QTabWidget>
#include <QTabBar>
#include <QContextMenuEvent>
#include <QtDebug>

class MainTab : public QTabWidget
{
Q_OBJECT
public:
	MainTab(QWidget* parent) : QTabWidget(parent)
	{
	}
public slots:
	void closeTab()
	{
		if(m_index > 3)
		{
			QWidget* w;
			
			w = widget(m_index);
			setCurrentIndex(0);
			removeTab(m_index);
			
			delete w;
		}
	}
	void closeAllTabs()
	{
		setCurrentIndex(0);
		
		for(int i=count()-1;i>3;i--)
		{
			QWidget* w = widget(i);
			removeTab(i);
			delete w;
		}
	}
protected:
	void contextMenuEvent(QContextMenuEvent* event)
	{
		QTabBar* bar = tabBar();
		
		m_index = bar->tabAt(bar->mapFrom(this, event->pos()));
		
		QAction* action;
		QMenu menu(this);
		
		if(m_index > 3)
		{
			action = menu.addAction(tr("Close tab"));
			connect(action, SIGNAL(triggered()), this, SLOT(closeTab()));
		}
		
		action = menu.addAction(tr("Close all tabs"));
		connect(action, SIGNAL(triggered()), this, SLOT(closeAllTabs()));
		
		menu.exec(mapToGlobal(event->pos()));
	}
	int m_index;
};

#endif
