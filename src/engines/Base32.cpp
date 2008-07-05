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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <cstring>

static const char base32_syms[32] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', '2', '3', '4', '5', '6', '7'
};
static unsigned char base32_map[256] = "\xff";

void base32_decode(const char source[32], unsigned char dest[20])
{
	if(base32_map[0] == 0xff)
	{
		memset(base32_map, 0, 256);
		for(int i=0;i<32;i++)
			base32_map[int(base32_syms[i]) & 0xff] = i;
	}
	
	for(int i=0;i<5;i++)
	{
		unsigned char s[8];
		unsigned char* d = dest+i*5;
		
		for(int x=0;x<8;x++)
			s[x] = base32_map[int(source[i*8+x]) & 0xff];
		
		d[0] = ((s[0] << 3) & 0xf8) | ((s[1] >> 2) & 0x07);
		d[1] = ((s[1] & 0x03) << 6) | ((s[2] & 0x1f) << 1) | ((s[3] >> 4) & 1);
		d[2] = ((s[3] & 0x0f) << 4) | ((s[4] >> 1) & 0x0f);
		d[3] = ((s[4] & 0x01) << 7) | ((s[5] & 0x1f) << 2) | ((s[6] >> 3) & 0x03);
		d[4] = ((s[6] & 0x07) << 5) | (s[7] & 0x1f);
	}
}

void base32_encode(const unsigned char source[20], char dest[32])
{
	for(int i=0;i<5;i++)
	{
		char* d = dest+i*8;
		const unsigned char* s = source+i*5;
		
		d[0] = (s[0] >> 3);
		d[1] = ((s[0] & 0x07) << 2) | (s[1] >> 6);
		d[2] = (s[1] >> 1) & 0x1f;
		d[3] = ((s[1] & 0x01) << 4) | (s[2] >> 4);
		d[4] = ((s[2] & 0x0f) << 1) | (s[3] >> 7);
		d[5] = (s[3] >> 2) & 0x1f;
		d[6] = ((s[3] & 0x03) << 3) | (s[4] >> 5);
		d[7] = s[4] & 0x1f;
		
		for(int x=0;x<8;x++)
			d[x] = base32_syms[int(d[x]) & 0x1f];
	}
}
