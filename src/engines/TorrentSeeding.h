#ifndef TORRENTSEEDING_H
#define TORRENTSEEDING_H
#include <QtGlobal>

namespace TorrentSeeding
{
	// Whether a finished torrent has reached its seed ratio limit. A limit of 0 means
	// stop as soon as the torrent is downloaded. The ratio is uploaded / downloaded;
	// a torrent that was complete when added has downloaded nothing, so it is
	// measured against its size instead.
	inline bool ratioReached(qint64 uploaded, qint64 downloaded, qint64 size, double limit)
	{
		if (limit <= 0.0)
			return true;
		const qint64 base = downloaded > 0 ? downloaded : size;
		if (base <= 0)
			return false;
		return double(uploaded) / double(base) >= limit;
	}
}
#endif
