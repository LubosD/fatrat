#include "TorrentProgressWidget.h"
#include <QPainter>
#include <QtDebug>

TorrentProgressWidget::TorrentProgressWidget(QWidget* parent) : QWidget(parent)
{
	m_data = new quint32[1000];
}

TorrentProgressWidget::~TorrentProgressWidget()
{
	delete [] m_data;
}

void TorrentProgressWidget::generate(const std::vector<bool>& data)
{
	m_image = generate(data, 1000, m_data);
	update();
}

QImage TorrentProgressWidget::generate(const std::vector<bool>& data, int width, quint32* buf, float sstart, float send)
{
	double fact = (data.size()-send-sstart)/float(width);
	double step = qMin<double>(255.0, 255.0/fact);
	
	for(int i=0;i<width;i++)
	{
		int from = i*fact+sstart;
		int to = (i+1)*fact+sstart;
		
		if(to >= data.size())
			to = data.size()-1;
		
		double color = 0;
		do
		{
			color += data[from] ? step : 0;
			from++;
		}
		while(from < to);
		
		int rcolor = 255 - qMin(color, 255.0);
		buf[i] = 0xff0000ff | (rcolor << 8) | (rcolor << 16);
	}
	
	return QImage((uchar*) buf, width, 1, QImage::Format_RGB32);
}

void TorrentProgressWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter;
	painter.begin(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setClipRegion(event->region());
	
	QImage bdraw = m_image.scaled(size());
	painter.drawImage(0, 0, bdraw);
	
	painter.end();
}
