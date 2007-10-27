#ifndef TORRENTPROGRESSWIDGET_H
#define TORRENTPROGRESSWIDGET_H
#include <QWidget>
#include <QImage>
#include <QPaintEvent>
#include <cmath>

class TorrentProgressWidget : public QWidget
{
Q_OBJECT
public:
	TorrentProgressWidget(QWidget* parent);
	~TorrentProgressWidget();
	void generate(const std::vector<bool>& data);
	void paintEvent(QPaintEvent* event);
private:
	QImage m_image;
	quint32* m_data;
};

#endif
