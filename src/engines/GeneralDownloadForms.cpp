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

#include "GeneralDownloadForms.h"

#include "Proxy.h"
#include "Settings.h"

HttpOptsWidget::HttpOptsWidget(QWidget* me, CurlDownload* myobj)
    : QObject(me), m_download(myobj) {
  setupUi(me);

  connect(pushUrlAdd, SIGNAL(clicked()), this, SLOT(addUrl()));
  connect(pushUrlEdit, SIGNAL(clicked()), this, SLOT(editUrl()));
  connect(pushUrlDelete, SIGNAL(clicked()), this, SLOT(deleteUrl()));
}

void HttpOptsWidget::load() {
  lineFileName->setText(m_download->m_strFile);

  m_urls = m_download->m_urls;
  foreach (UrlClient::UrlObject obj, m_urls) {
    QUrl copy = obj.url;
    copy.setUserInfo(QString());

    listUrls->addItem(copy.toString());
  }
}

bool HttpOptsWidget::accept() { return !lineFileName->text().contains('/'); }

void HttpOptsWidget::accepted() {
  QString newFileName = lineFileName->text();

  if (newFileName != m_download->m_strFile) {
    m_download->setTargetName(newFileName);
    m_download->m_bAutoName = false;
  }

  // walk through the operations and make them happen
  for (int i = 0; i < m_operations.size(); i++) {
    Operation& op = m_operations[i];
    switch (op.operation) {
      case Operation::OpAdd:
        m_download->m_urls << op.object;
        break;
      case Operation::OpEdit:
        m_download->m_urls[op.index] = op.object;

        if (m_download->isActive()) {
          int stopped = 0;
          for (int i = 0; i < m_download->m_segments.size(); i++) {
            CurlDownload::Segment& s = m_download->m_segments[i];
            if (s.client && s.urlIndex == op.index) {
              m_download->stopSegment(i, true);
              i = 0;
              stopped++;
            }
          }
          while (stopped--) m_download->startSegment(op.index);
        }
        break;
      case Operation::OpDelete:
        m_download->m_urls.removeAt(op.index);

        if (m_download->isActive()) {
          for (int i = 0; i < m_download->m_segments.size(); i++) {
            CurlDownload::Segment& s = m_download->m_segments[i];
            if (s.client && s.urlIndex == op.index) {
              m_download->stopSegment(i);
              i = 0;
            }
          }
        }
        for (QList<int>::iterator it = m_download->m_listActiveSegments.begin();
             it != m_download->m_listActiveSegments.end(); it++) {
          if (*it == op.index)
            it = m_download->m_listActiveSegments.erase(it);
          else if (*it > op.index)
            *it--;
        }
        break;
    }
  }

  // m_download->m_urls = m_urls;
}

void HttpOptsWidget::addUrl() {
  HttpUrlOptsDlg dlg((QWidget*)parent());

  if (dlg.exec() == QDialog::Accepted) {
    UrlClient::UrlObject obj;

    obj.url = dlg.m_strURL;
    obj.url.setUserName(dlg.m_strUser);
    obj.url.setPassword(dlg.m_strPassword);
    obj.strReferrer = dlg.m_strReferrer;
    obj.ftpMode = dlg.m_ftpMode;
    obj.proxy = dlg.m_proxy;
    obj.strBindAddress = dlg.m_strBindAddress;

    listUrls->addItem(dlg.m_strURL);
    m_urls << obj;

    Operation op;
    op.operation = Operation::OpAdd;
    op.object = obj;
    m_operations << op;
  }
}

void HttpOptsWidget::editUrl() {
  int row = listUrls->currentRow();
  HttpUrlOptsDlg dlg((QWidget*)parent());

  if (row < 0) return;

  UrlClient::UrlObject& obj = m_urls[row];

  QUrl temp = obj.url;
  temp.setUserInfo(QString());
  dlg.m_strURL = temp.toString();
  dlg.m_strUser = obj.url.userName();
  dlg.m_strPassword = obj.url.password();
  dlg.m_strReferrer = obj.strReferrer;
  dlg.m_ftpMode = obj.ftpMode;
  dlg.m_proxy = obj.proxy;
  dlg.m_strBindAddress = obj.strBindAddress;

  if (dlg.exec() == QDialog::Accepted) {
    obj.url = dlg.m_strURL;
    obj.url.setUserName(dlg.m_strUser);
    obj.url.setPassword(dlg.m_strPassword);
    obj.strReferrer = dlg.m_strReferrer;
    obj.ftpMode = dlg.m_ftpMode;
    obj.proxy = dlg.m_proxy;
    obj.strBindAddress = dlg.m_strBindAddress;

    Operation op;
    op.operation = Operation::OpEdit;
    op.object = obj;
    op.index = row;
    m_operations << op;

    listUrls->item(row)->setText(dlg.m_strURL);
  }
}

void HttpOptsWidget::deleteUrl() {
  int row = listUrls->currentRow();

  if (row >= 0) {
    delete listUrls->takeItem(row);
    m_urls.removeAt(row);

    Operation op;
    op.operation = Operation::OpDelete;
    op.index = row;
    m_operations << op;
  }
}

////////////////////////////////////////////////////////////////////////////////

HttpUrlOptsDlg::HttpUrlOptsDlg(QWidget* parent, QList<Transfer*>* multi)
    : QDialog(parent), m_ftpMode(UrlClient::FtpActive), m_multi(multi) {
  setupUi(this);

  if (m_multi != 0) {
    lineUrl->setVisible(false);
    labelUrl->setVisible(false);
  }
}

void HttpUrlOptsDlg::init() {
  QList<Proxy> listProxy = Proxy::loadProxys();

  comboFtpMode->addItems(QStringList(tr("Active mode")) << tr("Passive mode"));
  comboFtpMode->setCurrentIndex(int(m_ftpMode));

  lineUrl->setText(m_strURL);
  lineReferrer->setText(m_strReferrer);
  lineUsername->setText(m_strUser);
  linePassword->setText(m_strPassword);
  lineAddrBind->setText(m_strBindAddress);

  comboProxy->addItem(tr("(none)", "No proxy"));
  comboProxy->setCurrentIndex(0);

  if (m_proxy.isNull() && m_multi != 0) {
    m_proxy =
        QUuid::fromString(getSettingsValue("httpftp/defaultproxy").toString());
  }

  for (int i = 0; i < listProxy.size(); i++) {
    comboProxy->addItem(listProxy[i].strName);
    if (listProxy[i].uuid == m_proxy) comboProxy->setCurrentIndex(i + 1);
  }
}

int HttpUrlOptsDlg::exec() {
  int result;
  init();

  result = QDialog::exec();

  if (result == QDialog::Accepted) {
    QList<Proxy> listProxy = Proxy::loadProxys();

    m_strURL = lineUrl->text();
    m_strReferrer = lineReferrer->text();
    m_strUser = lineUsername->text();
    m_strPassword = linePassword->text();
    m_strBindAddress = lineAddrBind->text();
    m_ftpMode = UrlClient::FtpMode(comboFtpMode->currentIndex());

    int ix = comboProxy->currentIndex();
    m_proxy = (!ix) ? QUuid() : listProxy[ix - 1].uuid;

    if (m_multi != 0) runMultiUpdate();
  }

  return result;
}

void HttpUrlOptsDlg::accept() {
  QString url = lineUrl->text();

  if (m_multi != 0 || url.startsWith("http://") || url.startsWith("ftp://")
#ifdef WITH_SFTP
      || url.startsWith("sftp://")
#endif
      || url.startsWith("https://"))
    QDialog::accept();
}

void HttpUrlOptsDlg::runMultiUpdate() {
  // let the heuristics begin
  foreach (Transfer* t, *m_multi) {
    UrlClient::UrlObject& obj = dynamic_cast<CurlDownload*>(t)->m_urls[0];
    if (obj.url.userInfo()
            .isEmpty())  // we will not override the "automatic login data"
    {
      obj.url.setUserName(m_strUser);
      obj.url.setPassword(m_strPassword);
    }

    obj.strReferrer = m_strReferrer;
    obj.ftpMode = m_ftpMode;
    obj.proxy = m_proxy;
    obj.strBindAddress = m_strBindAddress;
  }
}
