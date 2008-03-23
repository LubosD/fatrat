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
	virtual void setObject(QString object) = 0;
	virtual QString object() const = 0;
	
	// TRANSFER STATES
	bool isActive() const;
	State state() const { return m_state; }
	virtual void setState(State newState);
	bool statePossible(State state) const;
	int retryCount() const { return m_nRetryCount; }
	qint64 timeRunning() const;
	
	// IDENTIFICATION AND INFORMATION
	virtual QString myClass() const = 0;
	virtual QString name() const = 0;
	virtual QString message() const { return QString(); }
	Mode mode() const { return m_mode; }
	virtual Mode primaryMode() const { return Download; } // because the BitTorrent transfer may switch modes at run-time
	virtual QString dataPath(bool bDirect) const;
	
	// TRANSFER SPEED AND SPEED LIMITS
	virtual void speeds(int& down, int& up) const = 0;
	void setUserSpeedLimits(int down,int up);
	void userSpeedLimits(int& down,int& up) const { down=m_nDownLimit; up=m_nUpLimit; }
	
	// TRANSFER SIZE
	virtual qulonglong total() const = 0;
	virtual qulonglong done() const = 0;
	
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
	QString comment() const { return m_strComment; }
	void setComment(QString text) { m_strComment = text; }
	
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
	WidgetHostChild* (*lpfnSettings)(QWidget*,QIcon&); // global settings
	QDialog* (*lpfnMultiOptions)(QWidget* /*parent*/, QList<Transfer*>& /*transfers*/); // mass proprerties changing
};


#endif
