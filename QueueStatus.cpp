#include "QueueStatus.h"
#include "fatrat.h"

QueueStatus::QueueStatus(QWidget* parent, Queue* queue)
	: QLabel(parent), m_queue(queue)
{
	setWindowFlags(Qt::ToolTip);
	setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
	//setMargin(1);
	setLineWidth(1);
	refresh();
	
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	connect(queue, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
	
	m_timer.start(1000);
	
	//move(QPoint(50,150));
	show();
}

void QueueStatus::refresh()
{
	// Active: %1 down, %2 up
	// Waiting: %1 down, %2 up
	// Speed: %1 down, %2 up
	
	QString text;
	int ad, au, wd, wu, sd, su;
	
	ad = au = wd = wu = sd = su = 0;
	
	m_queue->lock();
	for(int i=0;i<m_queue->size();i++)
	{
		Transfer* t = m_queue->at(i);
		Transfer::State state = t->state();
		const bool up = t->mode() == Transfer::Upload;
		int sup, sdown;
		
		if(state == Transfer::Active || state == Transfer::ForcedActive)
			(up ? au : ad)++;
		else if(state == Transfer::Waiting)
			(up ? wu : wd)++;
		
		t->speeds(sdown, sup);
		sd += sdown;
		su += sup;
	}
	m_queue->unlock();
	
	//text = QString("<font color=green>Active:</font> %1 down | %2 up<br>"
	//		"<font color=red>Waiting:</font> %3 down | %4 up<br>"
	//		"<font color=blue>Speed:</font> %5 down | %6 up")
	//		.arg(ad).arg(au).arg(wd).arg(wu).arg(formatSize(sd, true)).arg(formatSize(su, true));
	text = QString("<table cellspacing=4><tr><td><font color=green>Active:</font></td><td>%1 down</td><td>%2 up</td></tr>"
			"<tr><td><font color=red>Waiting:</font></td><td>%3 down</td><td>%4 up</td></tr>"
			"<tr><td><font color=blue>Speed:</font></td><td>%5 down</td><td>%6 up</td></tr></table>")
			.arg(ad).arg(au).arg(wd).arg(wu).arg(formatSize(sd, true)).arg(formatSize(su, true));
	setText(text);
	resize(sizeHint());
}

void QueueStatus::mousePressEvent(QMouseEvent*)
{
	hide();
}

