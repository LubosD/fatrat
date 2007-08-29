#ifndef _INFOBAR_H
#define _INFOBAR_H
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include "Transfer.h"

class InfoBar : public QLabel
{
Q_OBJECT
public:
	InfoBar(QWidget* parent, Transfer* d);
	virtual ~InfoBar();

	void mousePressEvent(QMouseEvent* event);
	//void mouseReleaseEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	
	static InfoBar* getInfoBar(Transfer* d);
	static void removeAll();
public slots:
	void downloadDestroyed(QObject*);
	void refresh();
private:
	int m_mx,m_my;
	Transfer* m_download;
	QTimer m_timer;
};

#endif
