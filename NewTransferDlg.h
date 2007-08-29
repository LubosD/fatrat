#ifndef NEWTRANSFERDLG_H
#define NEWTRANSFERDLG_H
#include <QWidget>
#include "ui_NewTransferDlg.h"
#include "Transfer.h"
#include <QDir>
#include <QFileDialog>
#include <QSettings>
#include <QtDebug>

class NewTransferDlg : public QDialog, Ui_NewTransferDlg
{
Q_OBJECT
public:
	NewTransferDlg(QWidget* parent) 
	: QDialog(parent), m_bDetails(false), m_bPaused(false), m_nDownLimit(0),
		   m_nUpLimit(0), m_nClass(-1), m_mode(Transfer::Download)
	{
		setupUi(this);

		textURIs->setFocus(Qt::OtherFocusReason);
		spinDown->setRange(0,INT_MAX);
		spinUp->setRange(0,INT_MAX);
		
		connect(toolBrowse, SIGNAL(pressed()), this, SLOT(browse()));
		//connect(textURIs, SIGNAL(textChanged()), this, SLOT(textChanged()));
		//connect(textFiles, SIGNAL(textChanged()), this, SLOT(textChanged()));
		connect(radioDownload, SIGNAL(clicked()), this, SLOT(switchMode()));
		connect(radioUpload, SIGNAL(clicked()), this, SLOT(switchMode()));
		connect(pushAddFiles, SIGNAL(clicked()), this, SLOT(browse2()));
		
		const EngineEntry* entries = Transfer::engines(Transfer::Download);
		comboClass->addItem(tr("Auto detect"));
		
		for(int i=0;entries[i].shortName;i++)
			comboClass->addItem(entries[i].longName);
		
		entries = Transfer::engines(Transfer::Upload);
		comboClass2->addItem(tr("Auto detect"));
		
		for(int i=0;entries[i].shortName;i++)
			comboClass2->addItem(entries[i].longName);
	}
	
	int exec()
	{
		int result;
		
		load();
		result = QDialog::exec();
		
		if(result == QDialog::Accepted)
			accepted();
		
		return result;
	}

	virtual void accept()
	{
		if(radioDownload->isChecked())
		{
			if((!m_bNewTransfer || !textURIs->toPlainText().isEmpty()) && !comboDestination->currentText().isEmpty())
			{
				QDir dir(comboDestination->currentText());
				if(dir.isReadable())
				{
					QDialog::accept();
					return;
				}
			}
		}
		else
			QDialog::accept();
	}
	void accepted()
	{
		QSettings settings;
		
		m_mode = radioDownload->isChecked() ? Transfer::Download : Transfer::Upload;
		if(m_mode == Transfer::Download)
		{
			m_strURIs = textURIs->toPlainText();
			m_strDestination = comboDestination->currentText();
			m_nClass = comboClass->currentIndex()-1;
			
			if(!m_lastDirs.contains(m_strDestination))
			{
				if(m_lastDirs.size() >= 5)
					m_lastDirs.removeFirst();
				m_lastDirs.append(m_strDestination);
				settings.setValue("lastdirs", m_lastDirs);
			}
		}
		else
		{
			m_strURIs = textFiles->toPlainText();
			m_strDestination = comboDestination2->currentText();
			m_nClass = comboClass2->currentIndex()-1;
		}
		
		m_bDetails = checkDetails->isChecked();
		m_bPaused = checkPaused->isChecked();
		m_nDownLimit = spinDown->value()*1024;
		m_nUpLimit = spinUp->value()*1024;
	}
	void load()
	{
		QSettings settings;
		
		if(!m_strClass.isEmpty())
		{
			if(m_lastDirs.size() >= 5)
				m_lastDirs.removeFirst();
			m_lastDirs.append(m_strDestination);
			settings.setValue("lastdirs", m_lastDirs);
		}
		{
			bool bFound = false;
			const EngineEntry* entries;
			
			entries = Transfer::engines(Transfer::Download);
			
			for(int i=0;entries[i].shortName;i++)
			{
				if(m_strClass == entries[i].shortName)
				{
					comboClass->setCurrentIndex(i+1);
					bFound = true;
					break;
				}
			}
			
			if(!bFound)
			{
				entries = Transfer::engines(Transfer::Upload);
			
				for(int i=0;entries[i].shortName;i++)
				{
					if(m_strClass == entries[i].shortName)
					{
						comboClass2->setCurrentIndex(i+1);
						break;
					}
				}
			}
		}
		
		if(m_strDestination.isEmpty())
			m_strDestination = settings.value("defaultdir", QDir::homePath()).toString();
		
		m_lastDirs = settings.value("lastdirs").toStringList();
		if(!m_lastDirs.contains(m_strDestination))
			comboDestination->addItem(m_strDestination);
		comboDestination->addItems(m_lastDirs);
		comboDestination->setEditText(m_strDestination);
		
		spinDown->setValue(m_nDownLimit/1024);
		spinUp->setValue(m_nUpLimit/1024);
		checkDetails->setChecked(m_bDetails);
		checkPaused->setChecked(m_bPaused);
		
		if(m_mode == Transfer::Upload)
		{
			stackedWidget->setCurrentIndex(1);
			radioUpload->setChecked(true);
			textFiles->setText(m_strURIs);
		}
		else
			textURIs->setText(m_strURIs);
	}
private slots:
	void browse()
	{
		QString dir = QFileDialog::getExistingDirectory(this, tr("Choose directory"), comboDestination->currentText());
		if(!dir.isNull())
			comboDestination->setEditText(dir);
	}
	void browse2()
	{
		QStringList files;
		
		files = QFileDialog::getOpenFileNames(this, tr("Choose files"));
		textFiles->append(files.join("\n"));
	}
	void textChanged()
	{
		QStringList list;
		
		if(radioDownload->isChecked())
			list = textURIs->toPlainText().split(QRegExp("\\s+"), QString::SkipEmptyParts);
		else
			list = textFiles->toPlainText().split(QRegExp("\\s+"), QString::SkipEmptyParts);
		checkDetails->setEnabled(list.size() <= 1);
	}
	void switchMode()
	{
		stackedWidget->setCurrentIndex(radioUpload->isChecked() ? 1 : 0);
	}
public:
	QString m_strURIs,m_strDestination,m_strClass;
	QStringList m_lastDirs;
	bool m_bDetails, m_bPaused, m_bNewTransfer;
	int m_nDownLimit, m_nUpLimit, m_nClass;
	Transfer::Mode m_mode;
};

#endif
