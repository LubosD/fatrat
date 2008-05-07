#ifndef _SETTINGSDLG_H
#define _SETTINGSDLG_H
#include <QDialog>
#include <QList>
#include <QVector>

#include "ui_SettingsDlg.h"
#include "Transfer.h"

class SettingsDlg : public QDialog, Ui_SettingsDlg
{
Q_OBJECT
public:
	SettingsDlg(QWidget* parent);
	~SettingsDlg();
	
	virtual void accept();
	int exec();
public slots:
	void buttonClicked(QAbstractButton* btn);
private:
	QList<WidgetHostChild*> m_children;
};

#endif
