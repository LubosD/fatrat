#include "InfoBar.h"
#include <QMouseEvent>
#include <QReadWriteLock>
#include <QList>
#include "fatrat.h"

static QReadWriteLock m_lock;
static QList<InfoBar*> m_bars;

InfoBar::InfoBar(QWidget* parent, Transfer* d) : QLabel(parent), m_download(d)
{
	setWindowFlags(Qt::ToolTip);
	//setMinimumWidth(350);
	//setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
	setFrameStyle(QFrame::Box);
	setMargin(2);
	
	connect(d, SIGNAL(destroyed(QObject*)), this, SLOT(downloadDestroyed(QObject*)));
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	
	m_timer.start(1000);
	
	m_lock.lockForWrite();
	m_bars << this;
	m_lock.unlock();
	
	refresh();
	move(QCursor::pos()-QPoint(5,5));
	show();
}

InfoBar::~InfoBar()
{
	m_lock.lockForWrite();
	m_bars.removeAll(this);
	m_lock.unlock();
}

void InfoBar::refresh()
{
	qulonglong done = m_download->done(), total = m_download->total();
	QString pc, speed, time = "- - -";
	
	if(m_download->isActive())
	{
		QString s;
		Transfer::Mode mode = m_download->primaryMode();
		int down,up;
		m_download->speeds(down,up);
		
		if(down || m_download->mode() == Transfer::Download)
		{
			speed = formatSize(down, true)+"<i>d</i>";
			
			if(mode == Transfer::Download && total)
				time = formatTime((total-done)/down);
		}
		if(up)
		{
			speed += ' ';
			speed += formatSize(up, true)+"<i>u</i>";
			
			if(mode == Transfer::Upload && total)
				time = formatTime((total-done)/up);
		}
	}
	if(speed.isEmpty())
			speed = "- - -";
	if(total)
		pc = QString("%1").arg(100.0/total*done, 0, 'f', 1);
	
	setText( QString("<font color=blue>N:</font> %1; <font color=blue>D:</font> %2%; "
			"<font color=blue>S:</font> %3; <font color=blue>T:</font> %4").arg(m_download->name()).arg(pc).arg(speed).arg(time) );
	
	QSize size = sizeHint();
	size.setWidth(std::max(size.width(),350));
	resize(size);
}

void InfoBar::mousePressEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		m_mx = event->globalX();
		m_my = event->globalY();
	}
	else if(event->buttons() == Qt::MidButton)
		delete this;
}

void InfoBar::mouseMoveEvent(QMouseEvent* event)
{
	if(event->buttons() == Qt::LeftButton)
	{
		move(x()+event->globalX()-m_mx, y()+event->globalY()-m_my);
		mousePressEvent(event);
	}
}

void InfoBar::downloadDestroyed(QObject*)
{
	close();
	deleteLater();
}

void InfoBar::removeAll()
{
	qDeleteAll(m_bars);
}

InfoBar* InfoBar::getInfoBar(Transfer* d)
{
	InfoBar* instance = 0;
	m_lock.lockForRead();
	
	foreach(InfoBar* b, m_bars)
	{
		if(b->m_download == d)
		{
			instance = b;
			break;
		}
	}
	
	m_lock.unlock();
	return instance;
}
