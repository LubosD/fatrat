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

#ifndef CURLUPLOAD_H
#define CURLUPLOAD_H
#include <curl/curl.h>

#include <QFile>
#include <QUrl>
#include <QUuid>

#include "CurlUser.h"
#include "Transfer.h"
#include "WidgetHostChild.h"
#include "fatrat.h"
#include "ui_FtpUploadOptsForm.h"

class CurlUpload : public Transfer, public CurlUser {
  Q_OBJECT
 public:
  CurlUpload();
  virtual ~CurlUpload();

  static Transfer* createInstance() { return new CurlUpload; }
  static int acceptable(QString url, bool bDrop);

  virtual void init(QString source, QString target);
  virtual void setObject(QString source);

  virtual void changeActive(bool nowActive);
  virtual void setSpeedLimits(int, int up);

  virtual QString object() const { return m_strSource; }
  virtual QString myClass() const { return "FtpUpload"; }
  virtual QString name() const { return m_strName; }
  virtual QString message() const { return m_strMessage; }
  virtual Mode primaryMode() const { return Upload; }
  virtual void speeds(int& down, int& up) const;
  virtual qulonglong total() const { return m_nTotal; }
  virtual qulonglong done() const;

  virtual void load(const QDomNode& map);
  virtual void save(QDomDocument& doc, QDomNode& map) const;
  virtual WidgetHostChild* createOptionsWidget(QWidget*);

  virtual void fillContextMenu(QMenu& menu);
  virtual QString remoteURI() const;
 protected slots:
  void computeHash();

 protected:
  virtual CURL* curlHandle();
  virtual void transferDone(CURLcode result);
  virtual size_t readData(char* buffer, size_t maxData);

  static int seek_function(QFile* file, curl_off_t offset, int origin);
  static int curl_debug_callback(CURL*, curl_infotype type, char* text,
                                 size_t bytes, CurlUpload* This);

 protected:
  CURL* m_curl;
  qint64 m_nDone, m_nTotal;
  QFile m_file;
  QString m_strSource, m_strMessage, m_strName, m_strBindAddress;
  QUrl m_strTarget;
  FtpMode m_mode;
  QUuid m_proxy;
  char m_errorBuffer[CURL_ERROR_SIZE];

  friend class FtpUploadOptsForm;
};

class FtpUploadOptsForm : public QObject,
                          public WidgetHostChild,
                          Ui_FtpUploadOptsForm {
  Q_OBJECT
 public:
  FtpUploadOptsForm(QWidget* me, CurlUpload* myobj);
  virtual void load();
  virtual void accepted();
  virtual bool accept();

 private:
  CurlUpload* m_upload;
};

#endif
