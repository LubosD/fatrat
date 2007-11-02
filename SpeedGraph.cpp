#include "SpeedGraph.h"
#include "fatrat.h"
#include <QtDebug>
#include <QSettings>

extern QSettings* g_settings;

SpeedGraph::SpeedGraph(QWidget* parent) : QWidget(parent), m_transfer(0)
{
	m_timer = new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(update()));
	m_timer->start(1000);
}

void SpeedGraph::setRenderSource(Transfer* t)
{
	if(m_transfer == t)
		return;
	disconnect(this, SLOT(setNull()));
	
	m_transfer = t;
	if(m_transfer)
		connect(m_transfer, SIGNAL(destroyed()), this, SLOT(setNull()));
	update();
}

void SpeedGraph::paintEvent(QPaintEvent* event)
{
	int top = 0;
	QQueue<QPair<int,int> > data;
	QPainter painter;
	int seconds = g_settings->value("graphminutes",int(5)).toInt()*60;
	
	painter.begin(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setClipRegion(event->region());
	painter.fillRect(event->rect(),QBrush(Qt::white));
	
	if(!m_transfer)
	{
		drawNoData(painter);
		painter.end();
		return;
	}
	
	data = m_transfer->speedData();
	
	for(int i=0;i<data.size();i++)
	{
		top = std::max(top, std::max(data[i].first,data[i].second));
	}
	if(!top || data.size()<2)
	{
		drawNoData(painter);
		painter.end();
		return;
	}
	
	top = std::max(top/10*11,10*1024);
	const int height = this->height();
	const int width = this->width();
	
	painter.setPen(QPen(Qt::gray, 1.0, Qt::DashLine));
	for(int i=1;i<10;i++)
	{
		int pos = int( float(height)/10.f*i );
		painter.drawLine(0,pos,width,pos);
		painter.drawText(0,pos-10,formatSize( qulonglong( top/10.f*(10-i) ), true));
	}
	
	const int elems = data.size()-1;
	qreal perpt = width / (qreal(std::max(elems,seconds))-1);
	qreal pos = width;
	QLineF* lines = new QLineF[elems+1];
	
	for(int i = 0;i<elems;i++) // download speed
	{
		lines[i] = QLineF(pos, height-height/qreal(top)*data[elems-i].first,
				pos-perpt, height-height/qreal(top)*data[elems-i-1].first);
		pos -= perpt;
	}
	
	painter.setPen(Qt::darkBlue);
	
	lines[elems] = QLineF(2,7,12,7);
	painter.drawLines(lines,elems+1);
	
	pos = width;
	for(int i = 0;i<elems;i++) // upload speed
	{
		lines[i] = QLineF(pos, height-height/qreal(top)*data[elems-i].second,
				pos-perpt, height-height/qreal(top)*data[elems-i-1].second);
		pos -= perpt;
	}
	
	painter.setPen(Qt::darkRed);
	
	lines[elems] = QLineF(2,19,12,19);
	painter.drawLines(lines,elems+1);
	delete [] lines;
	
	painter.setPen(QColor(80,180,80));
	for(int i=0;i<4;i++)
	{
		int x = width-(i+1)*(width/4);
		painter.drawLine(x, height, x, height-15);
		painter.drawText(x+2, height-2, tr("%1 mins ago").arg( (seconds/4) * (i+1) / 60.0 ));
	}
	
	painter.setPen(Qt::black);
	painter.drawText(15,12,tr("Download"));
	painter.drawText(15,24,tr("Upload"));
	
	painter.end();
}

void SpeedGraph::drawNoData(QPainter& painter)
{
	QFont font = painter.font();
	
	font.setPixelSize(20);
	
	painter.setFont(font);
	painter.setPen(Qt::gray);
	painter.drawText(contentsRect(), Qt::AlignHCenter | Qt::AlignVCenter, tr("NO DATA"));
}
