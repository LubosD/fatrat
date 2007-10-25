#ifndef COMMENTFORM_H
#define COMMENTFORM_H

#include "ui_CommentForm.h"

class CommentForm : public WidgetHostChild, Ui_CommentForm
{
public:
	CommentForm(QWidget* me, Transfer* t)
	: m_transfer(t)
	{
		setupUi(me);
	}
	
	virtual void load()
	{
		textEdit->setPlainText(m_transfer->comment());
	}
	
	virtual void accepted()
	{
		m_transfer->setComment(textEdit->toPlainText());
	}
private:
	Transfer* m_transfer;
};

#endif
