#ifndef MAINTAB_H
#define MAINTAB_H
#include <QMenu>
#include <QTabWidget>
#include <QTabBar>
#include <QContextMenuEvent>
#include <QToolButton>

class MainTab : public QTabWidget
{
Q_OBJECT
public:
	MainTab(QWidget* parent);
public slots:
	void closeTab();
	void closeTabBtn();
	void closeAllTabs();
	void openAppTool();
	void changeTabTitle(QString newTitle);
	void currentTabChanged(int newTab);
protected:
	void initAppTools(QMenu* tabOpenMenu);
	void contextMenuEvent(QContextMenuEvent* event);
	
	int m_index;
	QToolButton* m_toolTabClose;
};

#endif
