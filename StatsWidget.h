#ifndef STATSWIDGET_H
#define STATSWIDGET_H
#include <QWidget>
#include <QTimer>

class StatsWidget : public QWidget
{
Q_OBJECT
public:
	StatsWidget(QWidget* parent);
public slots:
	void refresh();
protected:
	virtual void paintEvent(QPaintEvent* event);
private:
	qint64 m_globDown, m_globUp;
	qint64 m_globDownPrev, m_globUpPrev;
	QTimer* m_timer;
	QString m_strInterface;
};

#endif
