/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#include "bencodeparser.h"
#include "connectionmanager.h"
#include "torrentclient.h"
#include "torrentserver.h"
#include "trackerclient.h"
#include "fatrat.h"

#include <QtCore>
#include <QApplication>

TrackerClient::TrackerClient(TorrentClient *downloader, QObject *parent)
		: /*QObject(parent),*/ torrentDownloader(downloader)
{
	moveToThread(QApplication::instance()->thread());
	length = 0;
	uploadedBytes = 0;
	downloadedBytes = 0;
	requestInterval = 5 * 60;
	requestIntervalTimer = -1;
	firstTrackerRequest = true;
	lastTrackerRequest = false;
	lastTrackerID = 0;

	connect(&http, SIGNAL(done(bool)), this, SLOT(httpRequestDone(bool)));
}

void TrackerClient::start(const MetaInfo &info)
{
	metaInfo = info;
	lastTrackerID = 0;

	QTimer::singleShot(0, this, SLOT(fetchPeerList()));

	if (metaInfo.fileForm() == MetaInfo::SingleFileForm)
	{
		length = metaInfo.singleFile().length;
	}
	else
	{
		QList<MetaInfoMultiFile> files = metaInfo.multiFiles();
		for (int i = 0; i < files.size(); ++i)
			length += files.at(i).length;
	}
	
	lastTrackerRequest = false;
}

void TrackerClient::stop()
{
	lastTrackerRequest = true;
	http.abort();
	fetchPeerList();
}

void TrackerClient::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == requestIntervalTimer)
	{
		if (http.state() == QHttp::Unconnected)
			fetchPeerList();
	}
	else
	{
		QObject::timerEvent(event);
	}
}

void TrackerClient::fetchPeerList()
{
	// Prepare connection details
	QStringList trackers = metaInfo.announceList();
	QUrl url;

	QByteArray infoHash = torrentDownloader->infoHash();
	unsigned int *messageDigest = (unsigned int *)infoHash.constData();

	// Percent encode the hash
	QByteArray encodedSum;
	const char hex[] = "0123456789abcdef";
	unsigned char chars[4];
	
	if(lastTrackerID >= trackers.size())
	{
		lastTrackerID = 0;
		return;
	}
	else
		url = trackers[lastTrackerID++];
	
	qDebug() << "Request to" << url;

	for (int i = 0; i < 5; ++i)
	{
		unsigned int digest = qntoh(messageDigest[i]);
		chars[0] = (digest & 0xff000000) >> 24;
		chars[1] = (digest & 0x00ff0000) >> 16;
		chars[2] = (digest & 0x0000ff00) >> 8;
		chars[3] = (digest & 0x000000ff);

		for (int j = 0; j < 4; ++j)
		{
			unsigned char c = chars[j];
			if ((c >= '0' && c <= '9')
			        || (c >= 'a' && c <= 'z')
			        || (c >= 'A' && c <= 'Z'))
				encodedSum += c;
			else
			{
				encodedSum += "%";
				encodedSum += hex[(c & 0xf0) >> 4];
				encodedSum += hex[c & 0xf];
			}
		}
	}

	bool seeding = (torrentDownloader->state() == TorrentClient::Seeding);
	QByteArray query;
	query += url.path().toLatin1();
	query += "?";
	query += "info_hash=" + encodedSum;
	query += "&peer_id=" + ConnectionManager::instance()->clientId();
	query += "&port=" + QByteArray::number(TorrentServer::instance()->serverPort());
	query += "&uploaded=" + QByteArray::number(uploadedBytes);
	query += "&downloaded=" + QByteArray::number(downloadedBytes);
	query += "&left="+ QByteArray::number(seeding ? 0 : qMax<int>(0, length - downloadedBytes));
	query += "&compact=1";
	if (seeding)
	{
		query += "&event=completed";
	}
	else if (firstTrackerRequest)
	{
		firstTrackerRequest = false;
		query += "&event=started";
	}
	else if (lastTrackerRequest)
	{
		query += "&event=stopped";
	}
	if (!trackerId.isEmpty())
		query += "&trackerid=" + trackerId;

	http.setHost(url.host(), url.port() == -1 ? 80 : url.port());
	if (!url.userName().isEmpty())
		http.setUser(url.userName(), url.password());

	http.get(query);
}

void TrackerClient::httpRequestDone(bool error)
{
	QTimer::singleShot(0, this, SLOT(fetchPeerList())); // repeat announce for all trackers
	
	if (lastTrackerRequest)
	{
		emit stopped();
		return;
	}

	if (error)
	{
		emit connectionError(http.error());
		return;
	}

	QByteArray response = http.readAll();
	http.abort();

	BencodeParser parser;
	if (!parser.parse(response))
	{
		qWarning("Error parsing bencode response from tracker: %s",
		         qPrintable(parser.errorString()));
		http.abort();
		return;
	}

	QMap<QByteArray, QVariant> dict = parser.dictionary();

	if (dict.contains("failure reason"))
	{
		// no other items are present
		emit failure(QString::fromUtf8(dict.value("failure reason").toByteArray()));
		return;
	}

	if (dict.contains("warning message"))
	{
		// continue processing
		emit warning(QString::fromUtf8(dict.value("warning message").toByteArray()));
	}

	if (dict.contains("tracker id"))
	{
		// store it
		trackerId = dict.value("tracker id").toByteArray();
	}

	if (dict.contains("interval"))
	{
		// Mandatory item
		if (requestIntervalTimer != -1)
			killTimer(requestIntervalTimer);
		requestIntervalTimer = startTimer(dict.value("interval").toInt() * 1000);
	}

	if (dict.contains("peers"))
	{
		// store it
		peers.clear();
		QVariant peerEntry = dict.value("peers");
		if (peerEntry.type() == QVariant::List)
		{
			QList<QVariant> peerTmp = peerEntry.toList();
			for (int i = 0; i < peerTmp.size(); ++i)
			{
				TorrentPeer tmp;
				QMap<QByteArray, QVariant> peer = qVariantValue<QMap<QByteArray, QVariant> >(peerTmp.at(i));
				tmp.id = QString::fromUtf8(peer.value("peer id").toByteArray());
				tmp.address.setAddress(QString::fromUtf8(peer.value("ip").toByteArray()));
				tmp.port = peer.value("port").toInt();
				peers << tmp;
			}
		}
		else
		{
			QByteArray peerTmp = peerEntry.toByteArray();
			for (int i = 0; i < peerTmp.size(); i += 6)
			{
				TorrentPeer tmp;
				uchar *data = (uchar *)peerTmp.constData() + i;
				tmp.port = (int(data[4]) << 8) + data[5];
				uint ipAddress = 0;
				ipAddress += uint(data[0]) << 24;
				ipAddress += uint(data[1]) << 16;
				ipAddress += uint(data[2]) << 8;
				ipAddress += uint(data[3]);
				tmp.address.setAddress(ipAddress);
				peers << tmp;
			}
		}
		emit peerListUpdated(peers);
	}
}
