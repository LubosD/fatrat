#ifndef GENERICOPTSFORM_H
#define GENERICOPTSFORM_H
#include "WidgetHostChild.h"
#include "ui_GenericOptsForm.h"
#include <QObject>
#include <QDir>
#include <QFileDialog>
#include <QtDebug>
#include "Transfer.h"

class GenericOptsForm : public QObject, public WidgetHostChild, Ui_GenericOptsForm
{
Q_OBJECT
private:
	GenericOptsForm();
public:
	GenericOptsForm(QWidget* parent)
	: m_parent(parent), m_mode(Transfer::Download)
	{
		setupUi(parent);
		connect(toolBrowse, SIGNAL(pressed()), this, SLOT(browse()));
	}
	
	virtual ~GenericOptsForm()
	{
	}
	
	virtual void load()
	{
		lineURI->setText(m_strURI);
		spinDown->setValue(m_nDownLimit);
		spinUp->setValue(m_nUpLimit);
		
		if(m_mode == Transfer::Upload)
			labelURI->setText(tr("Source:"));
	}
	
	virtual bool accept()
	{
		if(m_mode == Transfer::Download)
		{
			QDir dir(lineURI->text());
			return dir.isReadable();
		}
		else
		{
			QFileInfo file(lineURI->text());
			return file.isReadable();
		}
	}
	
	virtual void accepted()
	{
		m_strURI = lineURI->text();
		m_nDownLimit = spinDown->value();
		m_nUpLimit = spinUp->value();
	}
public slots:
	void browse()
	{
		QString uri;
		
		if(m_mode == Transfer::Upload)
			uri = QFileDialog::getOpenFileName(m_parent->parentWidget(), tr("Choose file"), lineURI->text());
		else
			uri = QFileDialog::getExistingDirectory(m_parent->parentWidget(), tr("Choose directory"), lineURI->text());
		if(!uri.isNull())
			lineURI->setText(uri);
	}
private:
	QStringList m_lastDirs;
	QWidget* m_parent;
public:
	QString m_strURI;
	int m_nUpLimit, m_nDownLimit;
	Transfer::Mode m_mode;
};

#endif
