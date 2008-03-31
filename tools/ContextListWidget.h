#ifndef CONTEXTLISTWIDGET_H
#define CONTEXTLISTWIDGET_H
#include <QMenu>
#include <QListWidget>

class ContextListWidget : public QListWidget
{
Q_OBJECT
public:
	ContextListWidget(QWidget* parent);
	void contextMenuEvent(QContextMenuEvent* event);
public slots:
	void addItem();
	void editItem();
	void deleteItem();
private:
	QMenu m_menu;
};

#endif
