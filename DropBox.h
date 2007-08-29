#ifndef DROPBOX_H
#define DROPBOX_H
#include <QLabel>

class DropBox : public QLabel
{
Q_OBJECT
public:
	DropBox(QWidget* parent);
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void dragEnterEvent(QDragEnterEvent *event);
	void dropEvent(QDropEvent *event);
private:
	int m_mx,m_my;
};

#endif
