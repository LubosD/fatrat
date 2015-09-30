/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "CaptchaQtDlg.h"

CaptchaQtDlg::CaptchaQtDlg(QWidget* parent)
	: QDialog(parent), m_nSecondsLeft(30)
{
	setupUi(this);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(secondElapsed()));
	connect(&m_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(imageLoaded(QNetworkReply*)));
	connect(lineText, SIGNAL(textChanged(const QString&)), this, SLOT(textChanged()));
}

void CaptchaQtDlg::load(QString url)
{
	m_strUrl = url;
	m_network.get(QNetworkRequest(url));
	updateInfo();
}

void CaptchaQtDlg::secondElapsed()
{
	m_nSecondsLeft--;
	updateInfo();

	if (!m_nSecondsLeft)
	{
		m_timer.stop();
		emit captchaEntered();
	}
}

void CaptchaQtDlg::updateInfo()
{
	labelInfo->setText(tr("Please re-type the characters seen in the picture below. This dialog will be automatically dismissed in <b>%1 seconds</b> (queue blocking prevention).").arg(m_nSecondsLeft));
}

void CaptchaQtDlg::imageLoaded(QNetworkReply* reply)
{
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		labelImage->setText(tr("Failed to load the captcha image."));
		m_timer.start(1000);
		return;
	}

	QImage img;
	if (img.load(reply, 0))
	{
		labelImage->setPixmap(QPixmap::fromImage(img));
		m_timer.start(1000);
	}
	else
	{
		labelImage->setText(tr("Failed to load the captcha image."));
	}
}

void CaptchaQtDlg::textChanged()
{
	m_nSecondsLeft = 30;
}

void CaptchaQtDlg::accept()
{
	emit captchaEntered(lineText->text());
	QDialog::accept();
}

void CaptchaQtDlg::reject()
{
	emit captchaEntered();
	QDialog::reject();
}
