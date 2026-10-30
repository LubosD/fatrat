#ifndef TORRENTEDGEPIECES_H
#define TORRENTEDGEPIECES_H
#include <QtGlobal>
#include <utility>

namespace TorrentEdgePieces
{
	// The first and last piece a file touches, or {-1,-1} for an empty file or a bad piece size.
	// Media containers keep their index at either end, so these are what a preview needs first.
	inline std::pair<int,int> ofFile(qint64 offset, qint64 size, qint64 pieceLength)
	{
		if (size <= 0 || pieceLength <= 0 || offset < 0)
			return { -1, -1 };
		return { int(offset / pieceLength), int((offset + size - 1) / pieceLength) };
	}
}
#endif
