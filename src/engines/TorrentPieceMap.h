/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

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


#ifndef TORRENTPIECEMAP_H
#define TORRENTPIECEMAP_H
#include <QtGlobal>
#include <vector>
#include <libtorrent/bitfield.hpp>

// Render one-pixel-high 0xAARRGGBB strips summarising a torrent's pieces
namespace TorrentPieceMap
{
	// blue colored
	void renderPieces(const libtorrent::bitfield& data, int width, quint32* buf, float sstart = 0, float send = 0);
	// grey colored
	void renderAvailability(const std::vector<int>& data, int width, quint32* buf, float sstart = 0, float send = -1);
}

#endif
