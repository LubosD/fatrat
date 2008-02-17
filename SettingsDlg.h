#ifndef _SETTINGSDLG_H
#define _SETTINGSDLG_H
#include <QDialog>
#include <QList>

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
protected:
	void fillEngines(const EngineEntry* engines);
private:
	QList<WidgetHostChild*> m_children;
};

#endif
