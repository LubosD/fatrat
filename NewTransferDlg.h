#ifndef NEWTRANSFERDLG_H
#define NEWTRANSFERDLG_H
#include <QWidget>
#include "ui_NewTransferDlg.h"
#include "Transfer.h"
#include "fatrat.h"
#include <QDir>

class NewTransferDlg : public QDialog, Ui_NewTransferDlg
{
Q_OBJECT
public:
	NewTransferDlg(QWidget* parent);
	int exec();
	void accept();
	void accepted();
	void load();
private slots:
	void browse();
	void browse2();
	void addObject();
	void switchMode();
	void authData();
public:
	QString m_strURIs,m_strDestination,m_strClass;
	QStringList m_lastDirs;
	Auth m_auth;
	bool m_bDetails, m_bPaused, m_bNewTransfer;
	int m_nDownLimit, m_nUpLimit, m_nClass;
	Transfer::Mode m_mode;
	int m_nQueue;
};

#endif
