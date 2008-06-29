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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
USA.
*/

#include "TorrentSettings.h"
#include "TorrentDownload.h"
#include <QDir>
#include <QMessageBox>
#include <QSettings>

extern const char* TORRENT_FILE_STORAGE;
extern QSettings* g_settings;

TorrentSettings::TorrentSettings(QWidget* w)
{
	setupUi(w);
	
	QStringList encs;
	
	encs << tr("Disabled") << tr("Enabled") << tr("Permit only encrypted");
	
	comboAllocation->addItems( QStringList() << tr("Full") << tr("Sparse") << tr("Compact") );
	comboEncIncoming->addItems(encs);
	comboEncOutgoing->addItems(encs);
	comboEncLevel->addItems( QStringList() << tr("Plaintext") << tr("RC4") << tr("Both", "Both levels") );
	
	connect(labelCleanup, SIGNAL(linkActivated(const QString&)), this, SLOT(cleanup()));
}

void TorrentSettings::load()
{
	spinListenStart->setValue(getSettingsValue("torrent/listen_start").toInt());
	spinListenEnd->setValue(getSettingsValue("torrent/listen_end").toInt());
	spinRatio->setValue(getSettingsValue("torrent/maxratio").toDouble());
	
	spinConnections->setValue(getSettingsValue("torrent/maxconnections").toInt());
	spinUploads->setValue(getSettingsValue("torrent/maxuploads").toInt());
	spinConnectionsLocal->setValue(getSettingsValue("torrent/maxconnections_loc").toInt());
	spinUploadsLocal->setValue(getSettingsValue("torrent/maxuploads_loc").toInt());
	
	spinFiles->setValue(getSettingsValue("torrent/maxfiles").toInt());
	checkDHT->setChecked(getSettingsValue("torrent/dht").toBool());
	checkPEX->setChecked(getSettingsValue("torrent/pex").toBool());
	comboAllocation->setCurrentIndex(getSettingsValue("torrent/allocation").toInt());
	
	comboEncIncoming->setCurrentIndex(getSettingsValue("torrent/enc_incoming").toInt());
	comboEncOutgoing->setCurrentIndex(getSettingsValue("torrent/enc_outgoing").toInt());
	
	comboEncLevel->setCurrentIndex(getSettingsValue("torrent/enc_level").toInt());
	checkEncRC4Prefer->setChecked(getSettingsValue("torrent/enc_rc4_prefer").toBool());
	
	lineIP->setText(getSettingsValue("torrent/external_ip").toString());
	
	comboProxyTracker->clear();
	comboProxyPeer->clear();
	comboProxySeed->clear();
	
	comboProxyTracker->addItem(tr("None", "No proxy"));
	comboProxyPeer->addItem(tr("None", "No proxy"));
	comboProxySeed->addItem(tr("None", "No proxy"));
	
	m_listProxy = Proxy::loadProxys();
	
	QUuid tracker, seed, peer;
	
	tracker = getSettingsValue("torrent/proxy_tracker").toString();
	seed = getSettingsValue("torrent/proxy_webseed").toString();
	peer = getSettingsValue("torrent/proxy_peer").toString();
	
	for(int i=0;i<m_listProxy.size();i++)
	{
		int index;
		QString name = m_listProxy[i].toString();
		
		comboProxyTracker->addItem(name);
		index = comboProxyTracker->count()-1;
		comboProxyTracker->setItemData(index, m_listProxy[i].uuid.toString());
		
		if(m_listProxy[i].uuid == tracker)
			comboProxyTracker->setCurrentIndex(index);
		
		comboProxySeed->addItem(name);
		index = comboProxySeed->count()-1;
		comboProxySeed->setItemData(index, m_listProxy[i].uuid.toString());
		
		if(m_listProxy[i].uuid == seed)
			comboProxySeed->setCurrentIndex(index);
		
		if(m_listProxy[i].nType != Proxy::ProxyHttp)
		{
			comboProxyPeer->addItem(name);
			index = comboProxyPeer->count()-1;
			comboProxyPeer->setItemData(index, m_listProxy[i].uuid.toString());
			
			if(m_listProxy[i].uuid == peer)
				comboProxyPeer->setCurrentIndex(index);
		}
	}
	
	checkUPNP->setChecked(getSettingsValue("torrent/mapping_upnp").toBool());
	checkNATPMP->setChecked(getSettingsValue("torrent/mapping_natpmp").toBool());
	checkLSD->setChecked(getSettingsValue("torrent/mapping_lsd").toBool());
}

void TorrentSettings::accepted()
{
	g_settings->setValue("torrent/listen_start", spinListenStart->value());
	g_settings->setValue("torrent/listen_end", spinListenEnd->value());
	g_settings->setValue("torrent/maxratio", spinRatio->value());
	
	g_settings->setValue("torrent/maxconnections", spinConnections->value());
	g_settings->setValue("torrent/maxuploads", spinUploads->value());
	g_settings->setValue("torrent/maxconnections_loc", spinConnectionsLocal->value());
	g_settings->setValue("torrent/maxuploads_loc", spinUploadsLocal->value());
	
	g_settings->setValue("torrent/maxfiles", spinFiles->value());
	g_settings->setValue("torrent/dht", checkDHT->isChecked());
	g_settings->setValue("torrent/pex", checkPEX->isChecked());
	g_settings->setValue("torrent/allocation", comboAllocation->currentIndex());
	
	g_settings->setValue("torrent/external_ip", lineIP->text());
	
	int index = comboProxyTracker->currentIndex();
	g_settings->setValue("torrent/proxy_tracker", comboProxyTracker->itemData(index).toString());
	index = comboProxySeed->currentIndex();
	g_settings->setValue("torrent/proxy_webseed", comboProxySeed->itemData(index).toString());
	index = comboProxyPeer->currentIndex();
	g_settings->setValue("torrent/proxy_peer", comboProxyPeer->itemData(index).toString());
	
	g_settings->setValue("torrent/enc_incoming", comboEncIncoming->currentIndex());
	g_settings->setValue("torrent/enc_outgoing", comboEncOutgoing->currentIndex());
	
	g_settings->setValue("torrent/enc_level", comboEncLevel->currentIndex());
	g_settings->setValue("torrent/enc_rc4_prefer", checkEncRC4Prefer->isChecked());
	
	g_settings->setValue("torrent/mapping_upnp", checkUPNP->isChecked());
	g_settings->setValue("torrent/mapping_natpmp", checkNATPMP->isChecked());
	g_settings->setValue("torrent/mapping_lsd", checkLSD->isChecked());
	
	TorrentDownload::applySettings();
}

void TorrentSettings::cleanup()
{
	int removed = 0;
	
	QDir dir = QDir::home();
	QStringList files, hashes, toremove;
	std::vector<libtorrent::torrent_handle> torrents;
	
	torrents = TorrentDownload::m_session->get_torrents();
	
	for(size_t i=0;i<torrents.size();i++)
	{
		const libtorrent::big_number& bn = torrents[i].info_hash();
		hashes << QByteArray((char*) bn.begin(), 20).toHex();
	}
	
	if(dir.cd(TORRENT_FILE_STORAGE))
		files = dir.entryList(QStringList("*.torrent"));
	
	foreach(QString file, files)
	{
		int pt, pos = file.lastIndexOf(" - ");
		if(pos < 0)
			continue;
		pt = file.lastIndexOf('.');
		if(pt < 0)
			continue; // shouldn't happen
		
		if(!hashes.contains(file.mid(pos+3, pt-pos-3)))
		{
			dir.remove(file);
			removed++;
		}
	}
	
	QMessageBox::information(spinListenStart, "FatRat", tr("Removed %1 files.").arg(removed));
}

