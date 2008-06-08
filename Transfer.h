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
*/

#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H
#include <QString>
#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QPair>
#include <QSettings>
#include <QDateTime>
#include <QDomNode>
#include "Logger.h"

struct EngineEntry;
class QObject;
class QWidget;
class WidgetHostChild;
class QIcon;
class QMenu;
class QDialog;

class Transfer : public Logger
{
	Q_OBJECT
public:
	Transfer(bool local = false);
	virtual ~Transfer();
	
	enum State { Waiting, Active, ForcedActive, Paused, Failed, Completed };
	
	// Note that Download oriented class may switch its mode to Upload!
	// Example: finished BitTorrent download switches to the seeding mode.
	enum Mode { ModeInvalid, Download, Upload };
	
	virtual void init(QString source, QString target) = 0;
	
	// For Upload-oriented classes: target = local file
	// For Download-oriented classes: target = local directory
	Q_INVOKABLE virtual void setObject(QString object) = 0;
	Q_INVOKABLE virtual QString object() const = 0;
	Q_PROPERTY(QString object WRITE setObject READ object)
	
	// TRANSFER STATES
	Q_INVOKABLE bool isActive() const;
	Q_PROPERTY(bool active READ isActive)
	
	State state() const;
	virtual void setState(State newState);
	Q_INVOKABLE QString stateString() const;
	Q_INVOKABLE void setStateString(QString s);
	Q_PROPERTY(QString state READ stateString WRITE setStateString)
	
	bool statePossible(State state) const;
	int retryCount() const { return m_nRetryCount; }
	qint64 timeRunning() const;
	
	// IDENTIFICATION AND INFORMATION
	Q_INVOKABLE virtual QString myClass() const = 0;
	Q_PROPERTY(QString myClass READ myClass)
	Q_INVOKABLE virtual QString name() const = 0;
	Q_PROPERTY(QString name READ name)
	Q_INVOKABLE virtual QString message() const { return QString(); }
	Q_PROPERTY(QString message READ message)
	Q_INVOKABLE Mode mode() const { return m_mode; }
	Q_PROPERTY(Transfer::Mode mode READ mode)
	Q_INVOKABLE virtual Mode primaryMode() const { return Download; } // because the BitTorrent transfer may switch modes at run-time
	Q_PROPERTY(Transfer::Mode primaryMode READ primaryMode)
	Q_INVOKABLE virtual QString dataPath(bool bDirect = true) const;
	Q_PROPERTY(QString dataPath READ dataPath)
	
	// TRANSFER SPEED AND SPEED LIMITS
	virtual void speeds(int& down, int& up) const = 0;
	Q_INVOKABLE void setUserSpeedLimits(int down,int up);
	void userSpeedLimits(int& down,int& up) const { down=m_nDownLimit; up=m_nUpLimit; }
	
	// TRANSFER SIZE
	Q_INVOKABLE virtual qulonglong total() const = 0;
	Q_PROPERTY(qulonglong total READ total)
	Q_INVOKABLE virtual qulonglong done() const = 0;
	Q_PROPERTY(qulonglong done READ done)
	
	// SERIALIZATION
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	
	// TRANSFER OPTIONS/DETAILS
	virtual WidgetHostChild* createOptionsWidget(QWidget*) { return 0; } // options (properties) of a particular download
	virtual QObject* createDetailsWidget(QWidget*) { return 0; } // detailed view
	virtual void fillContextMenu(QMenu&) { }
	
	// LOGGING
	QQueue<QPair<int,int> > speedData() const { return m_qSpeedData; }
	
	// COMMENT
	Q_INVOKABLE QString comment() const { return m_strComment; }
	Q_INVOKABLE void setComment(QString text) { m_strComment = text; }
	Q_PROPERTY(QString comment WRITE setComment READ comment)
	
	// AUTO ACTIONS
	QString autoActionCommand(State state) const;
	void setAutoActionCommand(State state, QString command);
	
	// GENERIC UTILITY FUNCTIONS
	static State string2state(QString s);
	static QString state2string(State s);
	static Transfer* createInstance(QString className);
	static Transfer* createInstance(Mode mode, int classID);
	static bool runProperties(QWidget* parent, Mode mode, int classID, QList<Transfer*> objects);
	
	struct BestEngine
	{
		BestEngine() : engine(0), type(ModeInvalid), nClass(-1) {}
		const EngineEntry* engine;
		Mode type;
		int nClass;
	};
	
	static const EngineEntry* engines(Mode type);
	static BestEngine bestEngine(QString uri, Mode type = ModeInvalid); // type == ModeInvalid => all types & drop search
	
	// SETTINGS UTILITY FUNCTIONS
	static QString getXMLProperty(const QDomNode& node, QString name);
	static void setXMLProperty(QDomDocument& doc, QDomNode& node, QString name, QString value);
	
	void internalSpeedLimits(int& down, int& up) const { down=m_nDownLimitInt; up = m_nUpLimitInt; }
signals:
	void stateChanged(Transfer::State prev, Transfer::State now);
	void modeChanged(Transfer::Mode prev, Transfer::Mode now);
public slots:
	void updateGraph();
	void retry();
protected:
	virtual void changeActive(bool nowActive) = 0;
	virtual void setSpeedLimits(int down,int up) = 0;
	void setInternalSpeedLimits(int down,int up);
	void setMode(Mode mode);
	void fireCompleted();
	
	State m_state;
	Mode m_mode;
	int m_nDownLimit,m_nUpLimit;
	int m_nDownLimitInt,m_nUpLimitInt;
	bool m_bLocal, m_bWorking;
	
	qint64 m_nTimeRunning;
	QDateTime m_timeStart;
	
	int m_nRetryCount;
	
	QString m_strLog, m_strComment, m_strCommandCompleted;
	
	QTimer* m_timer;
	QQueue<QPair<int,int> > m_qSpeedData;
	
	friend class QueueMgr;
	friend class Queue;
};

class TransferNotifier : public QObject
{
Q_OBJECT
public:
	TransferNotifier();
	static TransferNotifier* instance();
signals:
	void stateChanged(Transfer* d, Transfer::State prev, Transfer::State now);
	void modeChanged(Transfer* d, Transfer::Mode prev, Transfer::Mode now);
	friend class Transfer;
};

struct EngineEntry
{
	const char* shortName;
	const char* longName;
	void (*lpfnInit)();
	void (*lpfnExit)();
	Transfer* (*lpfnCreate)();
	int (*lpfnAcceptable)(QString, bool);
	QDialog* (*lpfnMultiOptions)(QWidget* /*parent*/, QList<Transfer*>& /*transfers*/); // mass proprerties changing
};

void initTransferClasses();
void addTransferClass(const EngineEntry& e, Transfer::Mode m);

#endif
