#ifndef _SETTINGSDLG_H
#define _SETTINGSDLG_H
#include <QDialog>
#include <QList>
#include <QSettings>
#include <QtDebug>

#include "ui_SettingsDlg.h"
#include "SettingsGeneralForm.h"
#include "SettingsDropBoxForm.h"
#include "SettingsProxyForm.h"
#include "MainWindow.h"
#include "Transfer.h"

extern QSettings* g_settings;

class SettingsDlg : public QDialog, Ui_SettingsDlg
{
Q_OBJECT
public:
	SettingsDlg(QWidget* parent) : QDialog(parent)
	{
		setupUi(this);
		
		QWidget* w = new QWidget(stackedWidget);
		
		m_children << (WidgetHostChild*)(new SettingsGeneralForm(w, this));
		listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/fatrat.png"), tr("Main"), listWidget) );
		stackedWidget->insertWidget(0, w);
		
		w = new QWidget(stackedWidget);
		m_children << (WidgetHostChild*)(new SettingsDropBoxForm(w, this));
		listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), tr("Drop-box"), listWidget) );
		stackedWidget->insertWidget(1, w);
		
		w = new QWidget(stackedWidget);
		m_children << (WidgetHostChild*)(new SettingsProxyForm(w, this));
		listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/proxy.png"), tr("Proxy"), listWidget) );
		stackedWidget->insertWidget(2, w);
		
		listWidget->setCurrentRow(0);
		
		fillEngines( Transfer::engines(Transfer::Download) );
		fillEngines( Transfer::engines(Transfer::Upload) );
		
		connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
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
				w->accepted();
			
			QDialog::accept();
			
			g_settings->sync();
			static_cast<MainWindow*>(getMainWindow())->reconfigure();
		}
	}
	
	int exec()
	{
		foreach(WidgetHostChild* w,m_children)
			w->load();
		
		return QDialog::exec();
	}
	
public slots:
	void buttonClicked(QAbstractButton* btn)
	{
		if(buttonBox->buttonRole(btn) == QDialogButtonBox::ApplyRole)
		{
			foreach(WidgetHostChild* w,m_children)
			{
				bool acc = w->accept();
				
				if(!acc)
					return;
			}
			
			foreach(WidgetHostChild* w,m_children)
				w->accepted();
			foreach(WidgetHostChild* w,m_children)
				w->load();
			
			static_cast<MainWindow*>(getMainWindow())->reconfigure();
		}
	}
protected:
	void fillEngines(const EngineEntry* engines)
	{
		int x = 2;
		
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
