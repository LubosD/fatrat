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

#ifndef JAVADOWNLOAD_H
#define JAVADOWNLOAD_H
#include "config.h"

#ifndef WITH_JPLUGINS
#error This file is not supposed to be included!
#endif

#include <QMap>
#include <QMutex>
#include <QTimer>

#include "CurlDownload.h"
#include "JavaPersistentVariables.h"
#include "WidgetHostChild.h"
#include "java/JObject.h"
#include "ui_JavaDownloadOptsForm.h"

class JDownloadPlugin;

class JavaDownload : public CurlDownload, protected JavaPersistentVariables {
  Q_OBJECT
 public:
  JavaDownload(const char* cls);
  virtual ~JavaDownload();

  static void globalInit();
  static void globalExit();
  static QStringList getConfigDialogs();

  virtual void init(QString source, QString target);
  virtual QString myClass() const;
  virtual QString name() const;

  virtual void load(const QDomNode& map);
  virtual void save(QDomDocument& doc, QDomNode& map) const;
  virtual void changeActive(bool bActive);
  virtual QObject* createDetailsWidget(QWidget* w) { return 0; }
  virtual WidgetHostChild* createOptionsWidget(QWidget* w);

  virtual void fillContextMenu(QMenu& menu) {}

  virtual QString object() const { return m_strTarget; }
  virtual void setObject(QString newdir);

  virtual qulonglong done() const;
  virtual void setState(State newState);
  virtual QString remoteURI() const;

  static Transfer* createInstance(const EngineEntry* e) {
    return new JavaDownload(e->shortName);
  }
  static int acceptable(QString uri, bool, const EngineEntry* e);

 protected:
  void deriveName();
  void startDownload(QString url, QList<QNetworkCookie> cookies,
                     QString referrer = QString(),
                     QString userAgent = QString(),
                     QString postData = QString());
  void setName(QString name) { m_strName = name; }

 private:
  QString m_strClass;
  QString m_strOriginal, m_strName, m_strTarget;
  QUrl m_downloadUrl;
  int m_nSecondsLeft;
  QUuid m_proxy;
  bool m_bHasLock, m_bTruncate;

  JDownloadPlugin* m_plugin;

  static QMap<QString, QMutex*> m_mutexes;

  struct JavaEngine {
    std::string name, shortName;
    QRegExp regexp;
    bool forceSingleTransfer, truncate;
    JObject ownAcceptable;
    QString configDialog;
  };

  static QMap<QString, JavaEngine> m_engines;

  friend class JPlugin;
  friend class JTransferPlugin;
  friend class JDownloadPlugin;
  friend class JavaDownloadOptsForm;
};

class JavaDownloadOptsForm : public QObject,
                             public WidgetHostChild,
                             Ui_JavaDownloadOptsForm {
  Q_OBJECT
 public:
  JavaDownloadOptsForm(QWidget* me, JavaDownload* myobj);
  virtual void load();
  virtual void accepted();
  virtual bool accept();

 private:
  JavaDownload* m_download;
};

#endif  // JAVADOWNLOAD_H
