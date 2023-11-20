/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#include "HashDlg.h"

#include <QFileDialog>
#include <QMessageBox>

HashDlg::HashDlg(QWidget* parent, QString file)
    : QDialog(parent), m_file(file), m_thread(&m_file) {
  setupUi(this);

  connect(&m_thread, SIGNAL(progress(int)), progressBar, SLOT(setValue(int)));
  connect(&m_thread, SIGNAL(finished(QByteArray)), this,
          SLOT(finished(QByteArray)));
  connect(pushCompute, SIGNAL(clicked()), this, SLOT(compute()));
  connect(toolBrowse, SIGNAL(clicked()), this, SLOT(browse()));

  comboHash->addItems(QStringList() << "MD4"
                                    << "MD5"
                                    << "SHA1");
  comboHash->setCurrentIndex(1);

  lineFile->setText(file);
}

HashDlg::~HashDlg() {
  if (m_thread.isRunning()) {
    m_thread.stop();
    m_thread.wait();
  }
}

void HashDlg::browse() {
  QString newfile;

  newfile =
      QFileDialog::getOpenFileName(this, tr("Choose file"), lineFile->text());

  if (!newfile.isNull()) {
    lineFile->setText(newfile);
    m_file.setFileName(newfile);
    progressBar->setValue(0);
  }
}

void HashDlg::compute() {
  if (m_thread.isRunning() || lineFile->text().isEmpty()) return;
  if (!m_file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, "FatRat", tr("Unable to open the file!"));
    return;
  }

  lineResult->clear();

  comboHash->setEnabled(false);
  pushCompute->setEnabled(false);
  pushCompute->setEnabled(false);

  progressBar->setValue(0);
  m_thread.setAlgorithm(
      (QCryptographicHash::Algorithm)comboHash->currentIndex());
  m_thread.start(QThread::LowestPriority);
}

void HashDlg::finished(QByteArray result) {
  QByteArray data = result.toHex();

  pushCompute->setEnabled(true);
  comboHash->setEnabled(true);
  pushCompute->setEnabled(true);

  lineResult->setText(data.data());

  m_file.close();
}

HashThread::HashThread(QFile* file) : m_file(file), m_bStop(false) {}

void HashThread::run() {
  QCryptographicHash hash(m_alg);
  QByteArray buf;
  qint64 total = m_file->size();
  int pcts = 0;

  m_bStop = false;

  do {
    int npcts;

    buf = m_file->read(1024);
    hash.addData(buf);

    npcts = 100.0 * m_file->pos() / double(total);

    if (npcts > pcts) {
      pcts = npcts;
      emit progress(pcts);
    }
  } while (buf.size() == 1024 && !m_bStop);

  if (!m_bStop) emit finished(hash.result());
}
