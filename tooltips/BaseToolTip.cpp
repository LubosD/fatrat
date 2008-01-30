#include "BaseToolTip.h"
#include <QApplication>
#include <QDesktopWidget>

BaseToolTip::BaseToolTip(QObject* master, QWidget* parent)
	: QLabel(parent)
{
	setWindowFlags(Qt::ToolTip);
	setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	setLineWidth(1);

	if(master != 0)
		connect(master, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(doRefresh()));
	
	m_timer.start(1000);
	QMetaObject::invokeMethod(this, "doRefresh", Qt::QueuedConnection);
}

void BaseToolTip::mousePressEvent(QMouseEvent*)
{
	hide();
}

void BaseToolTip::placeMe() // from Qt
{
	QPoint pos = QCursor::pos();
	int s = QApplication::desktop()->screenNumber(pos);
	QRect screen = QApplication::desktop()->screenGeometry(s);
	
	QPoint p = pos;
	p += QPoint(2, 16);
	if (p.x() + this->width() > screen.x() + screen.width())
		p.rx() -= 4 + this->width();
	if (p.y() + this->height() > screen.y() + screen.height())
		p.ry() -= 24 + this->height();
	if (p.y() < screen.y())
		p.setY(screen.y());
	if (p.x() + this->width() > screen.x() + screen.width())
		p.setX(screen.x() + screen.width() - this->width());
	if (p.x() < screen.x())
		p.setX(screen.x());
	if (p.y() + this->height() > screen.y() + screen.height())
		p.setY(screen.y() + screen.height() - this->height());
	
	move(p);
}
