#include "DropBox.h"
#include <QPixmap>
#include <QBitmap>
#include <QMouseEvent>
#include <QMainWindow>
#include <QSettings>
#include <QPainter>
#include "MainWindow.h"
#include "fatrat.h"
#include <QtDebug>

extern MainWindow* g_wndMain;
extern QSettings* g_settings;

DropBox::DropBox(QWidget* parent) : QWidget(parent)
{
	setWindowFlags(Qt::ToolTip);
	setAcceptDrops(true);
	
	m_renderer = new QSvgRenderer(QString(":/svg/mousetrap.svg"), this);
	move( g_settings->value("dropbox/position", QPoint(100,100)).toPoint() );
	
	reconfigure();
}

void DropBox::reconfigure()
{
	int myheight = g_settings->value("dropbox/height", getSettingsDefault("dropbox/height")).toInt();
	resize(myheight, myheight);
	
	m_buffer = QPixmap(size());
	
	QPainter p(&m_buffer);
	p.setViewport(0, 0, width(), height());
	p.eraseRect(0, 0, width(), height());
	m_renderer->render(&p);
	
	setMask(m_buffer.createMaskFromColor(Qt::white));
	
	m_bUnhide = g_settings->value("dropbox/unhide", getSettingsDefault("dropbox/unhide")).toBool();
}

void DropBox::paintEvent(QPaintEvent*)
{
	QPainter pt(this);
	pt.drawPixmap(0, 0, m_buffer);
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
	g_settings->setValue("dropbox/position", pos());
}

void DropBox::dragEnterEvent(QDragEnterEvent *event)
{
	g_wndMain->dragEnterEvent(event);
}

void DropBox::dropEvent(QDropEvent *event)
{
	if(m_bUnhide)
		g_wndMain->unhide();
	g_wndMain->dropEvent(event);
}
