/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "SettingsWebForm.h"
#include "Settings.h"
#include "HttpService.h"
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include "config.h"
#include "CertGenDlg.h"

SettingsWebForm::SettingsWebForm(QWidget* w, QObject* parent)
	: QObject(parent)
{
	setupUi(w);

	connect(toolBrowsePEM, SIGNAL(clicked()), this, SLOT(browsePem()));
	connect(pushGeneratePEM, SIGNAL(clicked()), this, SLOT(generatePem()));
}

void SettingsWebForm::load()
{
	checkEnable->setChecked(getSettingsValue("remote/enable").toBool());
	spinPort->setValue(getSettingsValue("remote/port").toInt());
	linePassword->setText(getSettingsValue("remote/password").toString());
	groupSSL->setChecked(getSettingsValue("remote/ssl").toBool());
	linePEM->setText(getSettingsValue("remote/ssl_pem").toString());
}

void SettingsWebForm::accepted()
{
	setSettingsValue("remote/enable", checkEnable->isChecked());
	setSettingsValue("remote/port", spinPort->value());
	setSettingsValue("remote/password", linePassword->text());
	setSettingsValue("remote/ssl", groupSSL->isChecked());
	setSettingsValue("remote/ssl_pem", linePEM->text());

	applySettings();
}

void SettingsWebForm::applySettings()
{
	HttpService::instance()->applySettings();
}

void SettingsWebForm::browsePem()
{
	QString file = QFileDialog::getOpenFileName(linePEM->parentWidget(), tr("Browse for PEM file"), QString(), tr("PEM files (*.pem)"));
	if (!file.isEmpty())
	{
		linePEM->setText(file);
	}
}

void SettingsWebForm::generatePem()
{
	const char* script = DATA_LOCATION "/data/genssl.sh";
	const char* config = DATA_LOCATION "/data/genssl.cnf";
	const char* pemfile = "/tmp/fatrat-webui.pem";

	CertGenDlg dlg(linePEM->parentWidget());

	if (dlg.exec() != QDialog::Accepted)
		return;

	QApplication::setOverrideCursor(Qt::WaitCursor);
	QProcess prc;

	qDebug() << "Starting: " << script << " " << config;
	prc.start(script, QStringList() << config << dlg.getHostname());
	prc.waitForFinished();

	QApplication::restoreOverrideCursor();

	if (prc.exitCode() != 0)
	{
		QMessageBox::critical(linePEM->parentWidget(), "FatRat", tr("Failed to generate a certificate, please ensure you have 'openssl' and 'sed' installed."));
		return;
	}

	QFile file(pemfile);
	if (!file.open(QIODevice::ReadOnly))
	{
		QMessageBox::critical(linePEM->parentWidget(), "FatRat", tr("Failed to generate a certificate, please ensure you have 'openssl' and 'sed' installed."));
		return;
	}

	QByteArray data = file.readAll();
	QDir::home().mkpath(USER_PROFILE_PATH "/data");

	QString path = QDir::homePath() + QLatin1String(USER_PROFILE_PATH) + "/data/fatrat-webui.pem";
	QFile out(path);

	if (!out.open(QIODevice::WriteOnly))
	{
		QMessageBox::critical(linePEM->parentWidget(), "FatRat", tr("Failed to open %1 for writing.").arg(path));
		return;
	}

	out.write(data);

	out.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
	out.close();
	file.remove();
	linePEM->setText(path);

	QMessageBox::information(linePEM->parentWidget(), "FatRat", tr("The certificate has been generated."));
}

