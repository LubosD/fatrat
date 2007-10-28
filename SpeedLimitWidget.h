#ifndef SPEEDLIMITWIDGET_H
#define SPEEDLIMITWIDGET_H
#include <QWidget>
#include "ui_SpeedLimitWidget.h"

class SpeedLimitWidget : public QWidget, Ui_SpeedLimitWidget
{
Q_OBJECT
public:
	SpeedLimitWidget(QWidget* parent)
	: QWidget(parent)
	{
		setupUi(this);
	}
};

#endif
