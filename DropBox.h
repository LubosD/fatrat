#ifndef DROPBOX_H
#define DROPBOX_H
#include <QLabel>
#include <QSvgRenderer>

class DropBox : public QWidget
{
Q_OBJECT
public:
	DropBox(QWidget* parent);
	void reconfigure();
protected:
	void paintEvent(QPaintEvent*);
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
private:
	int m_mx,m_my;
	QPixmap m_buffer;
	QSvgRenderer* m_renderer;
	bool m_bUnhide;
};

#endif
