#include "RapidshareUpload.h"
#include "RuntimeException.h"
#include "WidgetHostDlg.h"
#include <QBuffer>
#include <QFileInfo>
#include <QSettings>

extern QSettings* g_settings;

const long CHUNK_SIZE = 10*1024*1024;
const char* MIME_BOUNDARY = "---------------------244245272FATRAT12354348";

RapidshareUpload::RapidshareUpload()
	: m_engine(0), m_http(0), m_buffer(0)
{
	m_query = QueryNone;
	m_nFileID = m_nKillID = -1;
	m_nDone = 0;
	m_bIDJustChecked = false;
	
	m_type = AccountType( g_settings->value("rapidshare/account").toInt() );
	m_strUsername = g_settings->value("rapidshare/username").toString();
	m_strPassword = g_settings->value("rapidshare/password").toString();
}

RapidshareUpload::~RapidshareUpload()
{
	if(m_engine)
		m_engine->destroy();
}

int RapidshareUpload::acceptable(QString url, bool)
{
	return 0; // because this class doesn't actually use URLs
}

void RapidshareUpload::init(QString source, QString target)
{
	setObject(source);
}

void RapidshareUpload::setObject(QString source)
{
	m_strSource = source;
	
	int slash = source.lastIndexOf('/');
	if(slash < 0)
		m_strName = source;
	else
		m_strName = source.mid(slash+1);
	m_nTotal = QFileInfo(m_strSource).size();
}

void RapidshareUpload::changeActive(bool nowActive)
{
	if(nowActive)
	{
		if(m_type != AccountNone && (m_strUsername.isEmpty() || m_strPassword.isEmpty()))
		{
			m_strMessage = tr("You have to enter your account information");
			setState(Failed);
			return;
		}
		
		if(m_strServer.isEmpty())
		{
			m_query = QueryServerID;
			m_http = new QHttp("rapidshare.com", 80);
			m_buffer = new QBuffer(m_http);
			
			m_buffer->open(QIODevice::ReadWrite);
			
			connect(m_http, SIGNAL(done(bool)), this, SLOT(queryDone(bool)));
			m_http->get("/cgi-bin/rsapi.cgi?sub=nextuploadserver_v1", m_buffer);
		}
		else
		{
			if(m_nFileID > 0 && m_nKillID > 0 && !m_bIDJustChecked)
			{
				// verify that the ID is still valid
				m_query = QueryFileInfo;
				m_http = new QHttp("rapidshare.com", 80);
				m_buffer = new QBuffer(m_http);
				
				m_buffer->open(QIODevice::ReadWrite);
				
				connect(m_http, SIGNAL(done(bool)), this, SLOT(queryDone(bool)));
				m_http->get(QString("/cgi-bin/rsapi.cgi?sub=checkincomplete_v1&fileid=%1&killcode=%2").arg(m_nFileID).arg(m_nKillID), m_buffer);
			}
			else
			{
				m_nTotal = QFileInfo(m_strSource).size();
				
				if(m_nDone < m_nTotal)
					beginNextChunk();
				else
					setState(Completed);
			}
		}
	}
	else
	{
		m_bIDJustChecked = false;
		if(m_http)
		{
			m_http->abort();
			m_http->deleteLater();
			m_http = 0;
		}
		if(m_engine)
		{
			m_engine->destroy();
			m_engine = 0;
		}
	}
}

void RapidshareUpload::beginNextChunk()
{
	QString header = QString("%1\r\nContent-Disposition: form-data; name=\"rsapi_v1\"\r\n\r\n1\r\n").arg(MIME_BOUNDARY);
	QString footer = QString("\r\n%1--\r\n").arg(MIME_BOUNDARY);
	const char* handler;
	
	if(m_nFileID > 0 && m_nKillID > 0 && m_nDone > 0)
	{
		header += QString("%1\r\nContent-Disposition: form-data; name=\"fileid\"\r\n\r\n%2\r\n"
				"%1\r\nContent-Disposition: form-data; name=\"killcode\"\r\n\r\n%3\r\n")
				.arg(MIME_BOUNDARY).arg(m_nFileID).arg(m_nKillID);
		handler = "uploadresume";
	}
	else
	{
		if(m_type != AccountNone)
		{
			const char* acctype = (m_type == AccountCollector) ? "freeaccountid" : "login";
			header += QString("%1\r\nContent-Disposition: form-data; name=\"%2\"\r\n\r\n%3\r\n"
					 "%1\r\nContent-Disposition: form-data; name=\"password\"\r\n\r\n%4\r\n")
					.arg(MIME_BOUNDARY).arg(acctype).arg(m_strUsername).arg(m_strPassword);
		}
		handler = "upload";
	}
	
	const char* state = 0;
	if(m_nDone+CHUNK_SIZE >= m_nTotal)
		state = "complete";
	else //if(m_nDone+CHUNK_SIZE < m_nTotal)
		state = "incomplete";
	
	if(state != 0)
	{
		header += QString("%1\r\nContent-Disposition: form-data; name=\"%2\"\r\n\r\n1\r\n")
				.arg(MIME_BOUNDARY).arg(state);
	}
	
	header += QString("%1\r\nContent-Disposition: form-data; name=\"filecontent\"; filename=\"%2\"\r\n\r\n")
			.arg(MIME_BOUNDARY).arg(m_strName);
	
	QString url = QString("http://%1/cgi-bin/%2.cgi").arg(m_strServer).arg(handler);
	m_engine = new HttpEngine(QUrl(url), QUrl(), QUuid());
	
	connect(m_engine, SIGNAL(finished(bool)), this, SLOT(postFinished(bool)), Qt::QueuedConnection);
	//connect(m_engine, SIGNAL(statusMessage(QString)), this, SLOT(changeMessage(QString)));
	connect(m_engine, SIGNAL(logMessage(QString)), this, SLOT(enterLogMessage(QString)));
	
	m_engine->addHeaderValue("Content-Type", QString("multipart/form-data; boundary=")+MIME_BOUNDARY);
	m_engine->setSegment(m_nDone, m_nDone + qMin<qint64>(CHUNK_SIZE, m_nTotal - m_nDone));
	m_engine->setRequestBody(header.toUtf8(), footer.toUtf8());
	m_engine->request(m_strSource, true, 0);
}

void RapidshareUpload::postFinished(bool error)
{
	if(error)
	{
		m_strMessage = m_engine->errorString();
		setState(Failed);
		
		m_engine->destroy();
		m_engine = 0;
	}
	else
	{
		m_nDone += qMin<qint64>(CHUNK_SIZE, m_nTotal - m_nDone);
		
		QRegExp reFileID("File1.1=http://rapidshare.com/files/(\\d+)/");
		QRegExp reKillID("\\?killcode=(\\d+)");
		QString link;
		QByteArray response = m_engine->getResponseBody();
		
		qDebug() << response;
		
		try
		{
			if(response.indexOf("forbiddenfiles=1") != -1)
				throw RuntimeException(tr("This file is forbidden to be shared"));
			
			if(response.indexOf("not found at RapidShare.com") != -1)
				throw RuntimeException(tr("Invalid username"));
			
			if(response.indexOf("Account found, but password is incorrect") != -1)
				throw RuntimeException(tr("Invalid password"));
			
			if(!response.startsWith("COMPLETE") && !response.startsWith("CHUNK"))
			{
				int ix = reFileID.indexIn(response);
				if(ix < 0)
					throw RuntimeException(tr("Failed to find the file ID"));
				m_nFileID = reFileID.cap(1).toLongLong();
				
				ix = reKillID.indexIn(response);
				if(ix < 0)
					throw RuntimeException(tr("Failed to find the kill ID"));
				m_nKillID = reKillID.cap(1).toLongLong();
			}
			
			m_engine->destroy();
			m_engine = 0;
			
			if(m_nDone == m_nTotal)
			{
				link = QString(" http://rapidshare.com/files/%1/%2").arg(m_nFileID).arg(m_strName);
				enterLogMessage(tr("Download link:") + link);
				
				if(m_type == AccountNone)
					enterLogMessage(tr("Kill link:") + link + QString("?killcode=%3").arg(m_nKillID));
				
				setState(Completed);
			}
			else if(isActive())
			{
				beginNextChunk();
			}
		}
		catch(const RuntimeException& e)
		{
			m_engine->destroy();
			m_engine = 0;
			m_nDone = 0;
			m_strMessage = e.what();
			setState(Failed);
		}
	}
}

void RapidshareUpload::queryDone(bool error)
{
	if(!m_buffer)
	{
		qDebug() << "This error should not happen.";
		return;
	}
	
	m_buffer->seek(0);
	
	if(m_query == QueryServerID)
	{
		if(error)
		{
			m_strMessage = tr("Failed to get server ID");
			setState(Failed);
		}
		else
		{
			m_strServer = QString("rs%1l3.rapidshare.com").arg(m_buffer->readLine().data());
			
			enterLogMessage(tr("Uploading to %1").arg(m_strServer));
		}
	}
	else if(m_query == QueryFileInfo)
	{
		if(error)
		{
			m_strMessage = tr("Failed to query resume information");
			setState(Failed);
		}
		else
		{
			m_bIDJustChecked = true;
			
			bool ok;
			QByteArray line = m_buffer->readLine();
			m_nDone = line.toLongLong(&ok);
			
			if(!ok)
			{
				// file ID invalid etc.
				m_strMessage = line;
				m_nDone = 0;
				m_nFileID = m_nKillID = -1;
				//setState(Failed);
			}
			else
				m_strMessage = tr("File ID %1 validated").arg(m_nFileID);
		}
	}
	
	m_query = QueryNone;
	m_http->deleteLater();
	m_http = 0;
	m_buffer = 0;
	
	if(!error && isActive())
		changeActive(true); // next stage
}

void RapidshareUpload::setSpeedLimits(int, int up)
{
	if(m_engine)
		m_engine->setLimit(up);
}

void RapidshareUpload::speeds(int& down, int& up) const
{
	down = 0;
	up = 0;
	
	if(m_engine)
		up = m_engine->speed();
}

qulonglong RapidshareUpload::done() const
{
	if(!isActive() || !m_engine)
		return m_nDone;
	else
		return m_nDone + m_engine->done();
}

void RapidshareUpload::load(const QDomNode& map)
{
	setObject( getXMLProperty(map, "source") );
	m_nDone = getXMLProperty(map, "done").toLongLong();
	m_nFileID = getXMLProperty(map, "fileID").toLongLong();
	m_nKillID = getXMLProperty(map, "killID").toLongLong();
	m_type = (AccountType) getXMLProperty(map, "account").toLongLong();
	m_strUsername = getXMLProperty(map, "username");
	m_strPassword = getXMLProperty(map, "password");
	
	Transfer::load(map);
}

void RapidshareUpload::save(QDomDocument& doc, QDomNode& map) const
{
	Transfer::save(doc, map);
	
	setXMLProperty(doc, map, "source", m_strSource);
	setXMLProperty(doc, map, "done", QString::number(m_nDone));
	setXMLProperty(doc, map, "fileID", QString::number(m_nFileID));
	setXMLProperty(doc, map, "killID", QString::number(m_nKillID));
	setXMLProperty(doc, map, "account", QString::number(int(m_type)));
	setXMLProperty(doc, map, "username", m_strUsername);
	setXMLProperty(doc, map, "password", m_strPassword);
}

WidgetHostChild* RapidshareUpload::createOptionsWidget(QWidget* w)
{
	return new RapidshareOptsWidget(w, this);
}

WidgetHostChild* RapidshareUpload::createSettingsWidget(QWidget* w,QIcon& icon)
{
	icon = QIcon(":/fatrat/rscom.png");
	return new RapidshareSettings(w);
}

QDialog* RapidshareUpload::createMultipleOptionsWidget(QWidget* parent, QList<Transfer*>& transfers)
{
	WidgetHostDlg* host = new WidgetHostDlg(parent);
	host->addChild(new RapidshareOptsWidget(host->getChildHost(), &transfers, host));
	
	return host;
}

////////////////////////////////////////////

RapidshareOptsWidget::RapidshareOptsWidget(QWidget* me, RapidshareUpload* myobj)
	: m_upload(myobj), m_multi(0)
{
	init(me);
}

RapidshareOptsWidget::RapidshareOptsWidget(QWidget* me, QList<Transfer*>* multi, QObject* parent)
	: QObject(parent), m_upload(0), m_multi(multi)
{
	init(me);
}

void RapidshareOptsWidget::init(QWidget* me)
{
	setupUi(me);
	
	labelSettings->setVisible(false);
	comboAccount->addItems( QStringList() << tr("No account") << tr("Collector's account") << tr("Premium account") );
}

void RapidshareOptsWidget::load()
{
	if(m_upload)
	{
		comboAccount->setCurrentIndex((int) m_upload->m_type);
		lineUsername->setText(m_upload->m_strUsername);
		linePassword->setText(m_upload->m_strPassword);
	}
	else
	{
		comboAccount->setCurrentIndex(g_settings->value("rapidshare/account").toInt());
		lineUsername->setText(g_settings->value("rapidshare/username").toString());
		linePassword->setText(g_settings->value("rapidshare/password").toString());
	}
}

void RapidshareOptsWidget::accepted()
{
	QList<Transfer*> tl;
	
	if(m_multi)
		tl = *m_multi;
	else
		tl << m_upload;
	
	foreach(Transfer* t, tl)
	{
		bool bChanged = false;
		RapidshareUpload* tu = (RapidshareUpload*) t;
		
		QString user, pass;
		RapidshareUpload::AccountType acc = RapidshareUpload::AccountType( comboAccount->currentIndex() );
		
		user = lineUsername->text();
		pass = linePassword->text();
		
		if(user != tu->m_strUsername)
		{
			bChanged = true;
			tu->m_strUsername = user;
		}
		if(pass != tu->m_strPassword)
		{
			bChanged = true;
			tu->m_strPassword = pass;
		}
		if(acc != tu->m_type)
		{
			bChanged = true;
			tu->m_type = acc;
		}
		
		if(bChanged)
		{
			tu->m_nDone = 0;
			tu->m_nFileID = tu->m_nKillID = -1;
			
			if(tu->isActive())
			{
				tu->changeActive(false);
				tu->changeActive(true);
			}
		}
	}
}

bool RapidshareOptsWidget::accept()
{
	if(!comboAccount->currentIndex())
		return true;
	else
		return !lineUsername->text().isEmpty() && !linePassword->text().isEmpty();
}

///////////////////////////////////////////////

RapidshareSettings::RapidshareSettings(QWidget* me)
{
	setupUi(me);
	comboAccount->addItems( QStringList() << tr("No account") << tr("Collector's account") << tr("Premium account") );
}

void RapidshareSettings::load()
{
	comboAccount->setCurrentIndex(g_settings->value("rapidshare/account").toInt());
	lineUsername->setText(g_settings->value("rapidshare/username").toString());
	linePassword->setText(g_settings->value("rapidshare/password").toString());
}

void RapidshareSettings::accepted()
{
	g_settings->setValue("rapidshare/account", comboAccount->currentIndex());
	g_settings->setValue("rapidshare/username", lineUsername->text());
	g_settings->setValue("rapidshare/password", linePassword->text());
}

bool RapidshareSettings::accept()
{
	if(!comboAccount->currentIndex())
		return true;
	else
		return !lineUsername->text().isEmpty() && !linePassword->text().isEmpty();
}
