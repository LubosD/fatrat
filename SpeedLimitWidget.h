#ifndef SPEEDLIMITWIDGET_H
#define SPEEDLIMITWIDGET_H
#include <QWidget>
#include <QTimer>
#include "MainWindow.h"
#include "Queue.h"
#include "fatrat.h"
#include "ui_SpeedLimitWidget.h"

extern MainWindow* g_wndMain;

class SpeedLimitWidget : public QWidget, Ui_SpeedLimitWidget
{
Q_OBJECT
public:
	SpeedLimitWidget(QWidget* parent)
	: QWidget(parent)
	{
		setupUi(this);
		m_timer.start(1000);
		connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
	}
public slots:
	void refresh()
	{
		Queue* q = g_wndMain->getCurrentQueue();
		if(q != 0)
		{
			int down,up;
			q->speedLimits(down,up);
			
			g_wndMain->doneQueue(q);
			
			if(down > 0)
				labelDown->setText(formatSize(down,true));
			else
				labelDown->setText(QString::fromUtf8("∞ kB/s"));
			
			if(up > 0)
				labelUp->setText(formatSize(up,true));
			else
				labelUp->setText(QString::fromUtf8("∞ kB/s"));
		}
	}
private:
	QTimer m_timer;
};

#endif
