/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

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
	QString m_strInterface;
};

#endif
