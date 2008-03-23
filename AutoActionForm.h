#ifndef AUTOACTIONFORM_H
#define AUTOACTIONFORM_H

#include "ui_AutoActionForm.h"

class AutoActionForm : public WidgetHostChild, Ui_AutoActionForm
{
public:
	AutoActionForm(QWidget* me, Transfer* t)
	: m_transfer(t)
	{
		setupUi(me);
	}
	
	virtual void load()
	{
		lineCommandCompleted->setText(m_transfer->autoActionCommand(Transfer::Completed));
	}
	
	virtual void accepted()
	{
		m_transfer->setAutoActionCommand(Transfer::Completed, lineCommandCompleted->text());
	}
private:
	Transfer* m_transfer;
};

#endif
