#include "fatrat.h"
#include "FtpUpload.h"
#include "HashDlg.h"
#include <stdexcept>
#include <QFileInfo>
#include <QMenu>
#include <QtDebug>

FtpUpload::FtpUpload()
	: m_engine(0), m_nUpLimit(0), m_nDone(0), m_mode(FtpPassive)
{
	Transfer::m_mode = Upload;
}

FtpUpload::~FtpUpload()
{
	if(m_engine != 0)
		m_engine->destroy();
}

int FtpUpload::acceptable(QString url)
{
	return (url.startsWith("ftp://")) ? 1 : 0;
}

void FtpUpload::init(QString source, QString target)
{
	QFileInfo finfo;
	
	if(source.startsWith("file:///"))
		source = source.mid(7);
	
	finfo = QFileInfo(source);
	
	if(!finfo.exists())
		throw std::runtime_error("File does not exist");
	
	m_nTotal = finfo.size();
	if(m_strName.isEmpty())
		m_strName = finfo.fileName();
	
	if(!target.startsWith("ftp://"))
		throw std::runtime_error("Invalid protocol for this upload class (FTP)");
	
	m_strTarget = target;
	m_strSource = source;
}

void FtpUpload::setObject(QString object)
{
	m_strSource = object;
}

void FtpUpload::changeActive(bool nowActive)
{
	if(nowActive)
	{
		m_strMessage = QString();
		
		m_engine = new FtpEngine(m_strTarget, m_proxy);
		connect(m_engine, SIGNAL(finished(void*,bool)), this, SLOT(finished(void*,bool)));
		connect(m_engine, SIGNAL(logMessage(QString)), this, SLOT(enterLogMessage(QString)));
		m_engine->setRemoteName(m_strName);
		m_engine->request(m_strSource, FtpEngine::FtpPut | (m_mode == FtpActive) ? FtpEngine::FtpActive : FtpEngine::FtpPassive);
	}
	else if(m_engine != 0)
	{
		safeDestroy();
	}
}

void FtpUpload::safeDestroy()
{
	FtpEngine* engine = m_engine;
	if(engine != 0)
	{
		m_engine = 0;
		engine->destroy();
	}
}

void FtpUpload::finished(void*,bool error)
{
	if(error)
	{
		m_strMessage = m_engine->errorString();
		setState(Failed);
	}
	else
	{
		setState(Completed);
	}
	
	safeDestroy();
}

void FtpUpload::status(QString str)
{
	m_strMessage = str;
}

qulonglong FtpUpload::done() const
{
	if(m_engine != 0)
	{
		qulonglong d = m_engine->done();
		if(d > 0)
			return m_nDone = d;
	}
	return m_nDone;
}

void FtpUpload::setSpeedLimits(int,int up)
{
	m_nUpLimit = up;
	if(m_engine != 0)
		m_engine->setLimit(up);
}

void FtpUpload::speeds(int& down, int& up) const
{
	up = down = 0;
	if(m_engine != 0)
		up = m_engine->speed();
}

void FtpUpload::load(const QDomNode& map)
{
	QString source, target;
	Transfer::load(map);
	
	source = getXMLProperty(map, "source");
	target = getXMLProperty(map, "target");
	m_strName = getXMLProperty(map, "name");
	m_mode = (FtpMode) getXMLProperty(map, "ftpmode").toInt();
	m_nDone = getXMLProperty(map, "done").toLongLong();
	m_proxy = getXMLProperty(map, "proxy");
	
	try
	{
		init(source, target);
	}
	catch(std::exception& e)
	{
		setState(Failed);
		m_strMessage = e.what();
	}
}

void FtpUpload::save(QDomDocument& doc, QDomNode& map)
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "source", m_strSource);
	setXMLProperty(doc, map, "target", m_strTarget);
	setXMLProperty(doc, map, "name", m_strName);
	setXMLProperty(doc, map, "ftpmode", QString::number(m_mode));
	setXMLProperty(doc, map, "done", QString::number(m_nDone));
	setXMLProperty(doc, map, "proxy", m_proxy.toString());
}

WidgetHostChild* FtpUpload::createOptionsWidget(QWidget* w)
{
	return new FtpUploadOptsForm(w, this);
}

void FtpUpload::fillContextMenu(QMenu& menu)
{
	QAction* a;
	
	a = menu.addAction(tr("Compute hash..."));
	connect(a, SIGNAL(triggered()), this, SLOT(computeHash()));
}

void FtpUpload::computeHash()
{
	HashDlg dlg(getMainWindow(), m_strSource);
	dlg.exec();
}

////////////////////////////////////////////////////////////

FtpUploadOptsForm::FtpUploadOptsForm(QWidget* me,FtpUpload* myobj)
	: m_upload(myobj)
{
	setupUi(me);
}

void FtpUploadOptsForm::load()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl temp, url;
	
	temp = url = m_upload->m_strTarget;
	temp.setUserInfo(QString());
	lineTarget->setText(temp.toString());
	lineUsername->setText(url.userName());
	linePassword->setText(url.password());
	
	comboFtpMode->addItems(QStringList(tr("Active mode")) << tr("Passive mode"));
	comboFtpMode->setCurrentIndex(int(m_upload->m_mode));
	
	comboProxy->addItem(tr("(none)", "No proxy"));
	comboProxy->setCurrentIndex(0);
	
	for(int i=0;i<listProxy.size();i++)
	{
		comboProxy->addItem(listProxy[i].strName);
		if(listProxy[i].uuid == m_upload->m_proxy)
			comboProxy->setCurrentIndex(i+1);
	}
	
	lineAddrBind->setText(m_upload->m_strBindAddress);
}

void FtpUploadOptsForm::accepted()
{
	QList<Proxy> listProxy = Proxy::loadProxys();
	QUrl url = lineTarget->text();
	
	url.setUserName(lineUsername->text());
	url.setPassword(linePassword->text());
	
	m_upload->m_strTarget = url.toString();
	int ix = comboProxy->currentIndex();
	m_upload->m_proxy = (!ix) ? QUuid() : listProxy[ix-1].uuid;
	
	m_upload->m_mode = FtpMode( comboFtpMode->currentIndex() );
	m_upload->m_strBindAddress = lineAddrBind->text();
}

bool FtpUploadOptsForm::accept()
{
	return lineTarget->text().startsWith("ftp://");
}


