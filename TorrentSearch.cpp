#include "TorrentSearch.h"
#include "RuntimeException.h"
#include "fatrat.h"
#include <QDir>
#include <QFile>
#include <QDomDocument>
#include <QMessageBox>
#include <QHttp>
#include <QBuffer>
#include <QUrl>
#include <QMap>
#include <QRegExp>
#include <QTextDocument>
#include <QtDebug>

TorrentSearch::TorrentSearch()
	: m_bSearching(false)
{
	setupUi(this);
	
	connect(pushSearch, SIGNAL(clicked()), this, SLOT(search()));
	
	loadEngines();
	
	foreach(Engine e, m_engines)
	{
		QListWidgetItem* item = new QListWidgetItem(e.name, listEngines);
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked);
		listEngines->addItem(item);
	}
	
	QStringList hdr = QStringList() << tr("Name") << tr("Size") << tr("Seeders") << tr("Leechers") << tr("Source");
	treeResults->setHeaderLabels(hdr);
}

void TorrentSearch::loadEngines()
{
	QDomDocument doc;
	QFile file;
	QDir dir(DATA_LOCATION);
	
	dir.cd("data");
	file.setFileName( dir.absoluteFilePath("btsearch.xml") );
	
	if(!file.open(QIODevice::ReadOnly) || !doc.setContent(&file))
	{
		QMessageBox::critical(getMainWindow(), "FatRat", tr("Failed to load BitTorrent search engine information."));
		return;
	}
	
	QDomElement n = doc.documentElement().firstChildElement("engine");
	while(!n.isNull())
	{
		Engine e;
		
		e.http = 0;
		e.buffer = 0;
		
		e.name = n.attribute("name");
		e.id = n.attribute("id");
		
		QDomElement sn = n.firstChildElement("param");
		while(!sn.isNull())
		{
			QString contents = sn.firstChild().toText().data();
			QString type = sn.attribute("name");
			
			if(type == "query")
				e.query = contents;
			else if(type == "beginning")
				e.beginning = contents;
			else if(type == "splitter")
				e.splitter = contents;
			else if(type == "ending")
				e.ending = contents;
			
			sn = sn.nextSiblingElement("param");
		}
		
		sn = n.firstChildElement("regexp");
		while(!sn.isNull())
		{
			RegExpParam param;
			QString type = sn.attribute("name");
			
			param.regexp = sn.firstChild().toText().data();
			param.field = sn.attribute("field", "0").toInt();
			param.match = sn.attribute("match", "0").toInt();
			
			e.regexps << QPair<QString, RegExpParam>(type, param);
			
			sn = sn.nextSiblingElement("regexp");
		}
		
		m_engines << e;
		
		n = n.nextSiblingElement("engine");
	}
}

void TorrentSearch::search()
{
	QString expr = lineExpr->text();
	
	if(expr.isEmpty())
		return;
	
	for(int i=0;i<listEngines->count();i++)
	{
		if(listEngines->item(i)->checkState() == Qt::Checked)
		{
			QUrl url = m_engines[i].query.arg(expr);
			
			m_engines[i].buffer = new QBuffer(this);
			m_engines[i].http = new QHttp(url.host(), url.port(80), this);
			
			m_engines[i].buffer->open(QIODevice::ReadWrite);
			
			connect(m_engines[i].http, SIGNAL(done(bool)), this, SLOT(searchDone(bool)));
			m_engines[i].http->get(url.path()+"?"+url.encodedQuery(), m_engines[i].buffer);
		}
	}
}

void TorrentSearch::parseResults(Engine* e)
{
	try
	{
		const QByteArray& data = e->buffer->data();
		QList<QByteArray> results;
		int end, start;
		
		start = data.indexOf(e->beginning);
		if(start < 0)
			throw RuntimeException("Error parsing search engine's response - 'start'");
		
		end = data.indexOf(e->ending, start);
		if(end < 0)
			throw RuntimeException("Error parsing search engine's response - 'end'");
		
		results = splitArray(data.mid(start, end-start), e->splitter);
		
		foreach(QByteArray ar, results)
		{
			QMap<QString, QString> map;
			
			for(int i=0;i<e->regexps.size();i++)
			{
				QRegExp re(e->regexps[i].second.regexp);
				int pos = 0;
				
				for(int k=0;k<e->regexps[i].second.match+1;k++)
				{
					pos = re.indexIn(ar, pos);
					
					if(pos < 0)
						throw RuntimeException(QString("Failed to match \"%1\" in \"%2\"").arg(e->regexps[i].first).arg(QString(ar)));
					else
						pos++; // FIXME
				}
				
				QTextDocument doc;
				doc.setHtml(re.cap(e->regexps[i].second.field+1));
				map[e->regexps[i].first] = doc.toPlainText();
			}
			
			QTreeWidgetItem* item = new QTreeWidgetItem(treeResults);
			item->setText(0, map["name"]);
			item->setText(1, map["size"]);
			item->setText(2, map["seeders"]);
			item->setText(3, map["leechers"]);
			item->setText(4, e->name);
			
			treeResults->addTopLevelItem(item);
		}
	}
	catch(const RuntimeException& e)
	{
		qDebug() << e.what();
	}
}

void TorrentSearch::searchDone(bool error)
{
	QHttp* http = (QHttp*) sender(); // this sucks
	Engine* e = 0;
	
	for(int i=0;i<m_engines.size();i++)
	{
		if(m_engines[i].http == http)
		{
			e = &m_engines[i];
			break;
		}
	}
	
	if(!e)
		return;
	
	http->deleteLater();
	e->http = 0;
	
	if(!error)
		parseResults(e);
	
	e->buffer->deleteLater();
	e->buffer = 0;
}

QList<QByteArray> TorrentSearch::splitArray(const QByteArray& src, QString sep)
{
	int split = 0;
	QList<QByteArray> out;
	
	while(true)
	{
		int n = src.indexOf(sep, split);
		
		if(n < 0)
			break;
		
		out << src.mid(split, n-split);
		split = n + sep.size();
	}
	
	return out;
}


