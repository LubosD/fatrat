#include "TorrentProgressWidget.h"
#include <QPainter>

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
	double fact = data.size()/1000.0;
	int step = (int) qMin<double>(255.0, 255.0/fact);
	
	for(int i=0;i<1000;i++)
	{
		double from = round(i*fact);
		double to = round((i+1)*fact);
		
		if(to >= data.size())
			to = data.size()-1;
		
		int color = 0;
		do
		{
			color += data.at(from) ? step : 0;
			from++;
		}
		while(from < to);
		
		color = 255 - qMin(color, 255);
		m_data[i] = 0xff0000ff | (color << 8) | (color << 16);
	}
	
	m_image = QImage((uchar*) m_data, 1000,1, QImage::Format_RGB32);
	
	update();
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
