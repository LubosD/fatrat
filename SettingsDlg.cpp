#include "config.h"
#include "fatrat.h"
#include "SettingsDlg.h"
#include "Settings.h"
#include "MainWindow.h"

#include <QSettings>

extern QSettings* g_settings;
extern QVector<EngineEntry> g_enginesDownload;
extern QVector<EngineEntry> g_enginesUpload;
extern QVector<SettingsItem> g_settingsPages;

SettingsDlg::SettingsDlg(QWidget* parent) : QDialog(parent)
{
	setupUi(this);
	
	QWidget* w;
	
	for(int i=0;i<g_settingsPages.size();i++)
	{
		w = new QWidget(stackedWidget);
		m_children << g_settingsPages[i].lpfnCreate(w, this);
		stackedWidget->addWidget(w);
		listWidget->addItem( new QListWidgetItem(g_settingsPages[i].icon, w->windowTitle(), listWidget) );
	}
	
	connect(buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
}

SettingsDlg::~SettingsDlg()
{
	qDeleteAll(m_children);
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
