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


#include "BmpWriter.h"
#include <QtEndian>

static void putLE16(QByteArray& out, int offset, quint16 v)
{
	qToLittleEndian<quint16>(v, out.data() + offset);
}

static void putLE32(QByteArray& out, int offset, quint32 v)
{
	qToLittleEndian<quint32>(v, out.data() + offset);
}

QByteArray encodeBmp(const quint32* pixels, int width, int height)
{
	const int infoHeaderSize = 40;
	const int dataOffset = 14 + infoHeaderSize;
	const int dataSize = width * height * 4;

	QByteArray out(dataOffset + dataSize, '\0');

	// BITMAPFILEHEADER
	out[0] = 'B';
	out[1] = 'M';
	putLE32(out, 2, out.size());
	putLE32(out, 10, dataOffset);

	// BITMAPINFOHEADER; a negative height stores rows top-down
	putLE32(out, 14, infoHeaderSize);
	putLE32(out, 18, quint32(width));
	putLE32(out, 22, quint32(-height));
	putLE16(out, 26, 1);
	putLE16(out, 28, 32);
	putLE32(out, 30, 0); // BI_RGB
	putLE32(out, 34, dataSize);

	// BI_RGB stores B, G, R, unused - which is 0xAARRGGBB written little-endian
	for (int i = 0; i < width * height; i++)
		putLE32(out, dataOffset + 4*i, pixels[i]);

	return out;
}
