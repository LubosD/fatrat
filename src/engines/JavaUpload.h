/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef JAVAUPLOAD_H
#define JAVAUPLOAD_H
#include "config.h"
#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include <QByteArray>
#include <QFile>
#include <QMap>
#include <QString>

#include "CurlUser.h"
#include "JavaPersistentVariables.h"
#include "Transfer.h"
#include "engines/StaticTransferMessage.h"
#include "java/JMap.h"
#include "java/JUploadPlugin.h"

class JavaUpload : public StaticTransferMessage<Transfer>,
                   public CurlUser,
                   protected JavaPersistentVariables {
  Q_OBJECT
 public:
  JavaUpload(const char* clsName);
  virtual ~JavaUpload();

  static void globalInit();
  static void applySettings();
  static Transfer* createInstance(const EngineEntry* e) {
    return new JavaUpload(e->shortName);
  }
  static int acceptable(QString url, bool, const EngineEntry* e);

  virtual void init(QString source, QString target);
  virtual void setObject(QString source);

  virtual void changeActive(bool nowActive);
  virtual void setSpeedLimits(int, int up);

  virtual QString object() const { return m_strSource; }
  virtual QString myClass() const { return m_strClass; }
  virtual QString name() const { return m_strName; }
  virtual QString message() const { return m_strMessage; }
  virtual Mode primaryMode() const { return Upload; }
  virtual void speeds(int& down, int& up) const;
  virtual qulonglong total() const { return m_nTotal; }
  virtual qulonglong done() const;

  virtual void load(const QDomNode& map);
  virtual void save(QDomDocument& doc, QDomNode& map) const;

 protected:
  void curlInit();

  virtual CURL* curlHandle();
  virtual void transferDone(CURLcode result);
  virtual size_t readData(char* buffer, size_t maxData);
  virtual bool writeData(const char* buffer, size_t bytes);

  static size_t process_header(const char* ptr, size_t size, size_t nmemb,
                               JavaUpload* This);

  struct MimePart {
    QString name, value;
    bool filePart;
  };

  void putDownloadLink(QString downloadLink, QString killLink);
  void startUpload(QString url, QList<MimePart>& parts, qint64 offset,
                   qint64 bytes);
 private slots:
  void checkResponse();

 private:
  struct JavaEngine {
    std::string name, shortName;
    JObject ownAcceptable;
    QString configDialog;
  };

  static QMap<QString, JavaEngine> m_engines;

  QString m_strClass, m_strSource, m_strName;
  JUploadPlugin* m_plugin;
  qint64 m_nTotal, m_nDone, m_nThisPart;
  CURL* m_curl;
  QFile m_file;
  QByteArray m_buffer;
  char m_errorBuffer[CURL_ERROR_SIZE];
  curl_httppost* m_postData;
  QMap<QString, QString> m_headers;
  char m_fileName[256];

  friend class JUploadPlugin;
};

#endif
