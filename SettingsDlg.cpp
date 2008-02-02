#include "SettingsDlg.h"
#include "SettingsGeneralForm.h"
#include "SettingsDropBoxForm.h"
#include "SettingsNetworkForm.h"
#include "MainWindow.h"

#include <QSettings>

extern QSettings* g_settings;

SettingsDlg::SettingsDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);
	
	QWidget* w = new QWidget(stackedWidget);
	
	m_children << (WidgetHostChild*)(new SettingsGeneralForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/fatrat.png"), tr("Main"), listWidget) );
	stackedWidget->addWidget(w);
	
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsDropBoxForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/fatrat/miscellaneous.png"), tr("Drop-box"), listWidget) );
	stackedWidget->addWidget(w);
	
	w = new QWidget(stackedWidget);
	m_children << (WidgetHostChild*)(new SettingsNetworkForm(w, this));
	listWidget->addItem( new QListWidgetItem(QIcon(":/menu/network.png"), tr("Network"), listWidget) );
	stackedWidget->addWidget(w);
	
	listWidget->setCurrentRow(0);
	
	fillEngines( Transfer::engines(Transfer::Download) );
	fillEngines( Transfer::engines(Transfer::Upload) );
	
	connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
}

void SettingsDlg::accept()
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

int SettingsDlg::exec()
{
	foreach(WidgetHostChild* w,m_children)
		w->load();
	
	return QDialog::exec();
}

void SettingsDlg::buttonClicked(QAbstractButton* btn)
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

void SettingsDlg::fillEngines(const EngineEntry* engines)
{
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
				stackedWidget->addWidget(w);
				listWidget->addItem( new QListWidgetItem(icon, w->windowTitle(), listWidget) );
				m_children << c;
			}
		}
	}
}
