#ifndef MYTRAYICON_H
#define MYTRAYICON_H
#include "tooltips/TrayToolTip.h"
#include <QtDebug>

class MyTrayIcon : public QSystemTrayIcon
{
public:
	MyTrayIcon(QWidget* parent) : QSystemTrayIcon(parent)
	{
		m_tip = new TrayToolTip;
	}
	
	~MyTrayIcon()
	{
		delete m_tip;
	}
	
	bool event(QEvent* e)
	{
		if(e->type() == QEvent::ToolTip)
		{
			m_tip->regMove();
			return true;
		}
		else
			return QSystemTrayIcon::event(e);
	}
private:
	TrayToolTip* m_tip;
};

#endif
