#include "HashDlg.h"
#include <QMessageBox>
#include <QFileDialog>

HashDlg::HashDlg(QWidget* parent, QString file)
	: QDialog(parent), m_file(file), m_thread(&m_file)
{
	setupUi(this);
	
	connect(&m_thread, SIGNAL(progress(int)), progressBar, SLOT(setValue(int)));
	connect(&m_thread, SIGNAL(finished(QByteArray)), this, SLOT(finished(QByteArray)));
	connect(pushCompute, SIGNAL(clicked()), this, SLOT(compute()));
	connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browse()));
	
	comboHash->addItems( QStringList() << "MD4" << "MD5" << "SHA1");
	comboHash->setCurrentIndex(1);
	
	lineFile->setText(file);
}

HashDlg::~HashDlg()
{
	if(m_thread.isRunning())
	{
		m_thread.terminate();
		m_thread.wait();
	}
}

void HashDlg::browse()
{
	QString newfile;
	
	newfile = QFileDialog::getOpenFileName(this, tr("Choose file"), lineFile->text());
	
	if(!newfile.isNull())
	{
		lineFile->setText(newfile);
		m_file.setFileName(newfile);
	}
}

void HashDlg::compute()
{
	if(m_thread.isRunning() || lineFile->text().isEmpty())
		return;
	if(!m_file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(this, "FatRat", tr("Unable to open the file!"));
		return;
	}
	
	lineResult->clear();
	
	comboHash->setEnabled(false);
	pushCompute->setEnabled(false);
	pushCompute->setEnabled(false);
	
	progressBar->setValue(0);
	m_thread.setAlgorithm((QCryptographicHash::Algorithm) comboHash->currentIndex());
	m_thread.start(QThread::LowestPriority);
}

void HashDlg::finished(QByteArray result)
{
	QByteArray data = result.toHex();
	
	pushCompute->setEnabled(true);
	comboHash->setEnabled(true);
	pushCompute->setEnabled(true);
	
	lineResult->setText(data.data());
	
	m_file.close();
}

HashThread::HashThread(QFile* file)
	: m_file(file)
{
}

void HashThread::run()
{
	QCryptographicHash hash(m_alg);
	QByteArray buf;
	qint64 total = m_file->size();
	int pcts = 0;
	
	setTerminationEnabled(true);
	
	do
	{
		int npcts;
		
		buf = m_file->read(1024);
		hash.addData(buf);
		
		npcts = 100.0*m_file->pos()/double(total);
		
		if(npcts > pcts)
		{
			pcts = npcts;
			emit progress(pcts);
		}
	}
	while(buf.size() == 1024);
	
	emit finished(hash.result());
}
