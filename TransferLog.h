#ifndef _TRANSFERLOG_H
#define _TRANSFERLOG_H
#include <QObject>
#include <QTextEdit>
#include "Transfer.h"

class TransferLog : public QObject
{
Q_OBJECT
public:
	TransferLog(QObject* parent, QTextEdit* widget)
	: QObject(parent), m_text(widget), m_last(0)
	{
	}
	void setLogSource(Transfer* t)
	{
		if(t == m_last)
			return;
		
		if(m_last != 0)
			disconnect(m_last, SIGNAL(logMessage(QString)), m_text, SLOT(append(const QString&)));
		m_last = t;
		
		if(t != 0)
		{
			m_text->setPlainText(t->logContents());
			connect(t, SIGNAL(logMessage(QString)), m_text, SLOT(append(const QString&)));
			connect(t, SIGNAL(destroyed()), this, SLOT(onDeleteSource()));
		}
		else
			m_text->clear();
	}
public slots:
	void onDeleteSource()
	{
		m_last = 0;
	}
private:
	QTextEdit* m_text;
	Transfer* m_last;
};

#endif
