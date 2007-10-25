#ifndef _DOWNLOAD_H
#define _DOWNLOAD_H
#include <QString>
#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QPair>
#include <QSettings>
#include <QDomNode>

struct EngineEntry;
class QObject;
class QWidget;
class WidgetHostChild;
class QIcon;
class QMenu;
class QDialog;

class Transfer : public QObject
{
Q_OBJECT
public:
	Transfer(bool local = false);
	virtual ~Transfer();
	
	enum State { Waiting, Active, ForcedActive, Paused, Failed, Completed };
	
	// Note that Download oriented class may switch its mode to Upload!
	// Example: finished BitTorrent download switches to the seeding mode.
	enum Mode { Download, Upload };
	
	virtual void init(QString source, QString target) = 0;
	
	// For Upload-oriented classes: target = local file
	// For Download-oriented classes: target = local directory
	virtual void setObject(QString object) = 0;
	virtual QString object() const = 0;
	
	// TRANSFER STATES
	bool isActive() const;
	State state() const { return m_state; }
	virtual void setState(State newState);
	bool statePossible(State state);
	
	// IDENTIFICATION AND INFORMATION
	virtual QString myClass() const = 0;
	virtual QString name() const = 0;
	virtual QString message() const { return QString(); }
	Mode mode() { return m_mode; }
	virtual Mode primaryMode() { return Download; } // because the BitTorrent transfer may switch modes at run-time
	
	// TRANSFER SPEED AND SPEED LIMITS
	virtual void speeds(int& down, int& up) const = 0;
	void setUserSpeedLimits(int down,int up);
	void userSpeedLimits(int& down,int& up) { down=m_nDownLimit; up=m_nUpLimit; }
	
	// TRANSFER SIZE
	virtual qulonglong total() const = 0;
	virtual qulonglong done() const = 0;
	
	// SERIALIZATION
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map);
	
	// TRANSFER OPTIONS/DETAILS
	virtual WidgetHostChild* createOptionsWidget(QWidget*) { return 0; } // options (properties) of a particular download
	virtual QObject* createDetailsWidget(QWidget*) { return 0; } // detailed view
	virtual void fillContextMenu(QMenu&) { }
	
	// LOGGING
	QString logContents() const { return m_strLog; }
	QQueue<QPair<int,int> > speedData() const { return m_qSpeedData; }
	
	// COMMENT
	QString comment() const { return m_strComment; }
	void setComment(QString text) { m_strComment = text; }
	
	// GENERIC UTILITY FUNCTIONS
	static State string2state(QString s);
	static QString state2string(State s);
	static Transfer* createInstance(QString className);
	static Transfer* createInstance(Mode mode, int classID);
	static bool runProperties(QWidget* parent, Mode mode, int classID, QList<Transfer*> objects);
	static const EngineEntry* engines(Mode type);
	
	// SETTINGS UTILITY FUNCTIONS
	static QString getXMLProperty(const QDomNode& node, QString name);
	static void setXMLProperty(QDomDocument& doc, QDomNode& node, QString name, QString value);
signals:
	void stateChanged(Transfer::State prev, Transfer::State now);
	void modeChanged(Transfer::Mode prev, Transfer::Mode now);
	
	void logMessage(QString msg);
public slots:
	void updateGraph();
	// This function is supposed to be used by child classes
	void enterLogMessage(QString msg);
protected:
	virtual void changeActive(bool nowActive) = 0;
	virtual void setSpeedLimits(int down,int up) = 0;
	void setInternalSpeedLimits(int down,int up);
	void setMode(Mode mode);
	
	State m_state;
	Mode m_mode;
	int m_nDownLimit,m_nUpLimit;
	bool m_bLocal;
	
	QString m_strLog, m_strComment;
	
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
	Transfer* (*lpfnCreate)();
	int (*lpfnAcceptable)(QString);
	WidgetHostChild* (*lpfnSettings)(QWidget*,QIcon&); // global settings
	QDialog* (*lpfnMultiOptions)(QWidget* /*parent*/, QList<Transfer*>& /*transfers*/); // mass proprerties changing
};


#endif
