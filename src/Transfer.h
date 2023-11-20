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

#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H
#include <QDateTime>
#include <QDomNode>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QUuid>

#include "Logger.h"

struct EngineEntry;
class QObject;
class QWidget;
class WidgetHostChild;
class QIcon;
class QMenu;
class QDialog;
class Queue;

class Transfer : public Logger {
  Q_OBJECT
 public:
  Transfer(bool local = false);
  virtual ~Transfer();

  enum State { Waiting, Active, ForcedActive, Paused, Failed, Completed };

  // Note that Download oriented class may switch its mode to Upload!
  // Example: finished BitTorrent download switches to the seeding mode.
  enum Mode { ModeInvalid, Download, Upload };

  virtual void init(QString source, QString target) = 0;

  // For Upload-oriented classes: object = local file
  // For Download-oriented classes: object = local directory
  Q_INVOKABLE virtual void setObject(QString object) = 0;
  Q_INVOKABLE virtual QString object() const = 0;
  Q_PROPERTY(QString object WRITE setObject READ object)

  Q_INVOKABLE virtual QString remoteURI() const { return QString(); }
  Q_PROPERTY(QString remoteURI READ remoteURI)

  // TRANSFER STATES
  Q_INVOKABLE bool isActive() const;
  Q_PROPERTY(bool active READ isActive)

  State state() const;
  virtual void setState(State newState);
  Q_INVOKABLE QString stateString() const;
  Q_INVOKABLE void setStateString(QString s);
  Q_PROPERTY(QString state READ stateString WRITE setStateString)

  int retryCount() const { return m_nRetryCount; }
  qint64 timeRunning() const;
  Q_PROPERTY(qint64 timeRunning READ timeRunning)

  // IDENTIFICATION AND INFORMATION
  Q_INVOKABLE virtual QString myClass() const = 0;
  Q_PROPERTY(QString myClass READ myClass)
  Q_INVOKABLE virtual QString name() const = 0;
  Q_PROPERTY(QString name READ name)
  Q_INVOKABLE virtual QString message() const { return QString(); }
  Q_PROPERTY(QString message READ message)
  Q_INVOKABLE Mode mode() const { return m_mode; }
  Q_PROPERTY(Transfer::Mode mode READ mode)
  Q_INVOKABLE virtual Mode primaryMode() const {
    return Download;
  }  // because the BitTorrent transfer may switch modes at run-time
  Q_PROPERTY(Transfer::Mode primaryMode READ primaryMode)
  // If direct is true: returns the path of the file being downloaded
  // If direct is false: returns the directory where that file is located
  Q_INVOKABLE virtual QString dataPath(bool bDirect = true) const;
  Q_PROPERTY(QString dataPath READ dataPath)

  // TRANSFER SPEED AND SPEED LIMITS
  virtual void speeds(int& down, int& up) const = 0;
  Q_INVOKABLE void setUserSpeedLimits(int down, int up);
  void userSpeedLimits(int& down, int& up) const {
    down = m_nDownLimit;
    up = m_nUpLimit;
  }

  // TRANSFER SIZE
  Q_INVOKABLE virtual qulonglong total() const = 0;
  Q_PROPERTY(qulonglong total READ total)
  Q_INVOKABLE virtual qulonglong done() const = 0;
  Q_PROPERTY(qulonglong done READ done)

  // SERIALIZATION
  virtual void load(const QDomNode& map);
  virtual void save(QDomDocument& doc, QDomNode& map) const;

  // TRANSFER OPTIONS/DETAILS
  virtual WidgetHostChild* createOptionsWidget(QWidget*) {
    return 0;
  }  // options (properties) of a particular download
  virtual QObject* createDetailsWidget(QWidget*) { return 0; }  // detailed view
  virtual void fillContextMenu(QMenu&) {}

  // LOGGING
  QQueue<QPair<int, int> > speedData() const { return m_qSpeedData; }

  // COMMENT
  Q_INVOKABLE QString comment() const { return m_strComment; }
  Q_INVOKABLE void setComment(QString text) { m_strComment = text; }
  Q_PROPERTY(QString comment WRITE setComment READ comment)

  // AUTO ACTIONS
  QString autoActionCommand(State state) const;
  void setAutoActionCommand(State state, QString command);

  Q_INVOKABLE QString uuid() const;
  Q_PROPERTY(QString uuid READ uuid)

  // GENERIC UTILITY FUNCTIONS
  static State string2state(QString s);
  static QString state2string(State s);
  static Transfer* createInstance(QString className);
  static Transfer* createInstance(Mode mode, int classID);
  static bool runProperties(QWidget* parent, Mode mode, int classID,
                            QList<Transfer*> objects);

  struct BestEngine {
    BestEngine() : engine(0), type(ModeInvalid), nClass(-1) {}
    const EngineEntry* engine;
    Mode type;
    int nClass;

    operator bool() const { return engine != 0; }
  };

  static const EngineEntry* engines(Mode type);
  static BestEngine bestEngine(
      QString uri,
      Mode type =
          ModeInvalid);  // type == ModeInvalid => all types & drop search
  static int getEngineID(QString _class, Mode type = ModeInvalid);
  static const char* getEngineName(int id, Mode type);

  // SETTINGS UTILITY FUNCTIONS
  static QString getXMLProperty(const QDomNode& node, QString name);
  static void setXMLProperty(QDomDocument& doc, QDomNode& node, QString name,
                             QString value);

  void internalSpeedLimits(int& down, int& up) const {
    down = m_nDownLimitInt;
    up = m_nUpLimitInt;
  }

  typedef QList<Transfer*> TransferList;
 signals:
  void stateChanged(Transfer::State prev, Transfer::State now);
  void modeChanged(Transfer::Mode prev, Transfer::Mode now);
 public slots:
  void retry();

 protected:
  virtual void changeActive(bool nowActive) = 0;
  virtual void setSpeedLimits(int down, int up);
  void setInternalSpeedLimits(int down, int up);
  void setMode(Mode mode);
  void fireCompleted();
  void updateGraph();

  // Calls this->deleteLater()
  Q_INVOKABLE void replaceItself(Transfer* newObject);
  Q_INVOKABLE void replaceItself(Transfer::TransferList newObjects);
  Queue* myQueue() const;

  State m_state, m_lastState;
  Mode m_mode;
  int m_nDownLimit, m_nUpLimit;
  int m_nDownLimitInt, m_nUpLimitInt;
  bool m_bLocal, m_bWorking;

  qint64 m_nTimeRunning;
  QDateTime m_timeStart;

  int m_nRetryCount;

  QString m_strLog, m_strComment, m_strCommandCompleted;

  QQueue<QPair<int, int> > m_qSpeedData;
  QUuid m_uuid;

  friend class QueueMgr;
  friend class Queue;
#ifdef WITH_JPLUGINS
  friend class JPlugin;
#endif
};

class TransferNotifier : public QObject {
  Q_OBJECT
 public:
  TransferNotifier();
  static TransferNotifier* instance();
 signals:
  void stateChanged(Transfer* d, Transfer::State prev, Transfer::State now);
  void modeChanged(Transfer* d, Transfer::Mode prev, Transfer::Mode now);
  friend class Transfer;
};

struct EngineEntry {
  const char* shortName;
  const char* longName;
  void (*lpfnInit)();
  void (*lpfnExit)();
  union {
    Transfer* (*lpfnCreate)();
    Transfer* (*lpfnCreate2)(const EngineEntry*);
  };
  union {
    // localSearch - only for Upload transfer classes
    // Normally a upload class would be called with a remote URL (where to
    // upload) If localsearch == true, then we're giving the function a local
    // file (where from upload) Used _only_ for drag&drop operation if a local
    // file is dropped
    int (*lpfnAcceptable)(QString, bool /*localSearch*/);
    int (*lpfnAcceptable2)(QString, bool /*localSearch*/, const EngineEntry*);
  };
  QDialog* (*lpfnMultiOptions)(
      QWidget* /*parent*/,
      QList<Transfer*>& /*transfers*/);  // mass proprerties changing
};

void initTransferClasses();
void addTransferClass(const EngineEntry& e, Transfer::Mode m);

#endif
