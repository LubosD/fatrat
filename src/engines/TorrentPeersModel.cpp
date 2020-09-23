/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

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

#include "TorrentDownload.h"
#include "TorrentPeersModel.h"
#include "fatrat.h"
#include <QIcon>
#include <libtorrent/peer_info.hpp>
#include <QtDebug>

extern void* g_pGeoIP;

extern const char* ( *GeoIP_country_name_by_addr_imp ) ( void*, const char* );
extern const char* ( *GeoIP_country_code_by_addr_imp ) ( void*, const char* );

static QMap<QString,QIcon> g_mapFlags;

TorrentPeersModel::TorrentPeersModel ( QObject* parent, TorrentDownload* d )
		: QAbstractListModel ( parent ), m_download ( d ), m_nLastRowCount ( 0 )
{
	m_columns << tr ( "IP address" ) << tr ( "Country" ) << tr ( "Client" );
	m_columns << tr ( "Encryption" ) << tr ( "Source" ) << tr ( "Download" ) << tr ( "Upload" );
	m_columns << tr ( "Downloaded" ) << tr ( "Uploaded" ) << tr ( "State" );
	m_columns << tr ( "Completed" );
}

QModelIndex TorrentPeersModel::index ( int row, int column, const QModelIndex &parent ) const
{
	if ( !parent.isValid() )
		return createIndex ( row,column, ( void* ) this );
	else
		return QModelIndex();
}

QModelIndex TorrentPeersModel::parent ( const QModelIndex& ) const
{
	return QModelIndex();
}

int TorrentPeersModel::rowCount ( const QModelIndex& ) const
{
	return m_nLastRowCount;
}

QVariant TorrentPeersModel::headerData ( int section, Qt::Orientation orientation, int role ) const
{
	if ( orientation == Qt::Horizontal && role == Qt::DisplayRole )
		return m_columns[section];
	return QVariant();
}

QVariant TorrentPeersModel::data ( const QModelIndex &index, int role ) const
{
	if ( index.row() >= ( int ) m_peers.size() )
		return QVariant();
	const libtorrent::peer_info& info = m_peers[index.row() ];

	if ( role == Qt::DisplayRole )
	{
		switch ( index.column() )
		{
			case 0:
			{
				std::string ip = info.ip.address().to_string();
				return QString ( ip.c_str() );
			}
			case 1:
			{
				const char* country = 0;
				std::string ip = info.ip.address().to_string();

				if ( g_pGeoIP != 0 )
					country = GeoIP_country_name_by_addr_imp ( g_pGeoIP, ip.c_str() );
				if ( country != 0 )
					return QString ( country );
				else
					return QString();
			}
			case 2:
				return QString::fromUtf8 ( info.client.c_str() );
			case 3:
				if(info.flags & libtorrent::peer_info::rc4_encrypted)
					return "RC4";
				else if(info.flags & libtorrent::peer_info::plaintext_encrypted)
					return "Plain";
				else
					return QVariant();
			case 4:
				switch(info.source)
				{
					case libtorrent::peer_info::tracker:
						return "Tracker";
					case libtorrent::peer_info::dht:
						return "DHT";
					case libtorrent::peer_info::pex:
						return "PEX";
					case libtorrent::peer_info::lsd:
						return "LSD";
					case libtorrent::peer_info::resume_data:
						return "Resume";
					case libtorrent::peer_info::incoming:
						return "Incoming";
					default:
					{
						if(info.flags & libtorrent::peer_info::local_connection)
							return QVariant();
						else
							return "Incoming";
					}
				}
			case 5:
				return formatSize ( info.down_speed, true );
			case 6:
				return formatSize ( info.up_speed, true );
			case 7:
				return formatSize ( info.total_download );
			case 8:
				return formatSize ( info.total_upload );
			case 9:
				if ( info.flags & libtorrent::peer_info::connecting )
					return tr ( "Connecting" );
				else if ( info.flags & libtorrent::peer_info::handshake )
					return tr ( "Handshaking" );
				else
				{
					QString text;
					if ( info.flags & libtorrent::peer_info::interesting )
						text += "Interesting ";
					if ( info.flags & libtorrent::peer_info::choked )
						text += "Choked ";
					if ( info.flags & libtorrent::peer_info::remote_interested )
						text += "Remote_interested ";
					if ( info.flags & libtorrent::peer_info::remote_choked )
						text += "Remote_choked ";
					if ( info.connection_type == libtorrent::peer_info::web_seed )
						text += "WEB_SEED";
					if ( info.flags & libtorrent::peer_info::on_parole )
						text += "On parole";

					return text;
				}
			case 10:
			{
				int pcs = 0;
				for ( ssize_t i=0;i<info.pieces.size();i++ )
					if ( info.pieces[i] )
						pcs++;
				QString pct = QString ( "%1%" ).arg ( ( int ) ( 100.0/double ( info.pieces.size() ) *pcs ) );
				
				if(info.flags & libtorrent::peer_info::seed)
					pct += " S";
				return pct;
			}
		}
	}
	else if ( role == Qt::DecorationRole )
	{
		if ( index.column() == 1 && g_pGeoIP != 0 )
		{
			std::string ip = info.ip.address().to_string();
			const char* country = GeoIP_country_code_by_addr_imp ( g_pGeoIP, ip.c_str() );

			if ( country != 0 )
			{
				char ct[3] = { (char) tolower ( country[0] ), (char) tolower ( country[1] ), 0 };

				if ( !g_mapFlags.contains ( ct ) )
				{
					char flag[] = ":/flags/xx.gif";
					flag[8] = ct[0];
					flag[9] = ct[1];
					g_mapFlags[ct] = QIcon ( flag );
				}
				return g_mapFlags[ct];
			}
		}
	}

	return QVariant();
}

bool TorrentPeersModel::hasChildren ( const QModelIndex & parent ) const
{
	return !parent.isValid();
}

void TorrentPeersModel::refresh()
{
	int count = 0;

	if ( m_download->m_handle.is_valid() )
	{
		m_peers.clear();
		m_download->m_handle.get_peer_info ( m_peers );
		count = m_peers.size();
	}

	if ( count > m_nLastRowCount )
	{
		beginInsertRows ( QModelIndex(), m_nLastRowCount, count-1 );
		endInsertRows();
	}
	else if ( count < m_nLastRowCount )
	{
		beginRemoveRows ( QModelIndex(), 0, m_nLastRowCount-count-1 );
		endRemoveRows();
	}
	m_nLastRowCount = count;

	dataChanged ( createIndex ( 0,0 ), createIndex ( count, m_columns.size() ) ); // refresh the view
}
