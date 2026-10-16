#pragma once
#include <QString>

namespace tui
{
	// Same output as the web UI's format.ts
	QString formatSize(qint64 bytes);
	QString formatSpeed(qint64 bytesPerSecond);
	QString formatEta(qint64 total, qint64 done, qint64 bytesPerSecond);
	QString formatDuration(double seconds);

	// Number of terminal cells a string takes (wide and combining characters aware)
	int displayWidth(const QString& s);
	// Shortens to at most width cells, ending in an ellipsis
	QString elide(const QString& s, int width, bool ascii = false);
	// Pads with spaces or truncates to exactly width cells
	QString fit(const QString& s, int width);
	QString fitRight(const QString& s, int width);
	QString repeat(const QString& s, int n);
}
