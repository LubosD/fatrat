#include "DropBox.h"
#include <QPixmap>
#include <QBitmap>
#include <QMouseEvent>
#include <QMainWindow>
#include <QSettings>
#include "MainWindow.h"
#include <QtDebug>

extern MainWindow* g_wndMain;

DropBox::DropBox(QWidget* parent) : QLabel(parent, Qt::FramelessWindowHint)
{
	QSettings settings;
	QPixmap pixmap(":/fatrat/dropbox.png");
	
	setWindowFlags(Qt::ToolTip);
	setPixmap(pixmap);
	setMask(pixmap.mask());
	resize(100,100);
	
	setAcceptDrops(true);
	
	move( settings.value("dropbox/position", QPoint(100,100)).toPoint() );
}
void DropBox::mousePressEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		m_mx = event->globalX();
		m_my = event->globalY();
	}
	else if(event->buttons() == Qt::MidButton)
		g_wndMain->trayIconActivated(QSystemTrayIcon::MiddleClick);
}

void DropBox::mouseMoveEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		move(x()+event->globalX()-m_mx, y()+event->globalY()-m_my);
		mousePressEvent(event);
	}
}

void DropBox::mouseReleaseEvent(QMouseEvent*)
{
	QSettings settings;
	settings.setValue("dropbox/position", pos());
}

void DropBox::dragEnterEvent(QDragEnterEvent *event)
{
	g_wndMain->dragEnterEvent(event);
}

void DropBox::dropEvent(QDropEvent *event)
{
	g_wndMain->dropEvent(event);
}
