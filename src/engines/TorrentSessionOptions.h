#ifndef TORRENTSESSIONOPTIONS_H
#define TORRENTSESSIONOPTIONS_H
#include <QString>
#include <QHostAddress>
#include <algorithm>

// Helpers that turn FatRat's torrent settings into libtorrent session settings.
namespace TorrentSessionOptions
{
	// The port to bind first. libtorrent retries the following ports (see listenRetries()).
	inline QString listenInterfaces(int start)
	{
		return QString("0.0.0.0:%1, [::]:%1").arg(start);
	}

	// libtorrent's max_retry_port_bind: how many ports after the first it tries
	// when binding fails, i.e. the bound port is the first free one in start..end.
	inline int listenRetries(int start, int end)
	{
		return std::max(0, std::min(end, 65535) - start);
	}

	// The announce_ip setting; empty if the user gave nothing or something that is not an IP address.
	inline QString announceIp(const QString& text)
	{
		QHostAddress addr;
		if (!addr.setAddress(text.trimmed()))
			return QString();
		return addr.toString();
	}
}
#endif
