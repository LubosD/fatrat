#ifndef _SETTINGSDLG_H
#define _SETTINGSDLG_H
#include <QDialog>
#include <QList>
#include "ui_SettingsDlg.h"
#include "SettingsGeneralForm.h"
#include "Transfer.h"

class SettingsDlg : public QDialog, Ui_SettingsDlg
{
Q_OBJECT
public:
	SettingsDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		
		QWidget* w = new QWidget(stackedWidget);
		
		m_children << (WidgetHostChild*)(new SettingsGeneralForm(w));
		listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/fatrat_brown.png"), tr("Main"), listWidget) );
		stackedWidget->insertWidget(0, w);
		
		listWidget->setCurrentRow(0);
		
		fillEngines( Transfer::engines(Transfer::Download) );
		fillEngines( Transfer::engines(Transfer::Upload) );
	}
	
	virtual void accept()
	{
		bool accepted = true, acc;
		
		foreach(WidgetHostChild* w,m_children)
		{
			acc = w->accept();
			accepted = accepted && acc;
			
			if(!accepted)
				break;
		}
		
		if(accepted)
		{
			foreach(WidgetHostChild* w,m_children)
			{
				w->accepted();
			}
			
			QDialog::accept();
		}
	}
	
	int exec()
	{
		foreach(WidgetHostChild* w,m_children)
		{
			w->load();
		}
		
		return QDialog::exec();
	}
protected:
	void fillEngines(const EngineEntry* engines)
	{
		int x = 0;
		
		for(int i=0;engines[i].shortName;i++)
		{
			if(engines[i].lpfnSettings)
			{
				QIcon icon;
				QWidget* w = new QWidget(stackedWidget);
				WidgetHostChild* c = engines[i].lpfnSettings(w,icon);
				
				if(c == 0)
				{
					// omfg
					delete w;
					continue;
				}
				else
				{
					stackedWidget->insertWidget(++x, w);
					listWidget->addItem( new QListWidgetItem(icon, w->windowTitle(), listWidget) );
					m_children << c;
				}
			}
		}
	}
private:
	QList<WidgetHostChild*> m_children;
};

#endif
