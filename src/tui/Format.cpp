#include "Format.h"
#include <QList>
#include <cmath>

namespace tui
{

QString formatSize(qint64 bytes)
{
	static const char* units[] = { "B", "KB", "MB", "GB", "TB" };
	if (bytes <= 0)
		return "0 B";

	double value = bytes;
	int unit = 0;
	while (value >= 1024 && unit < 4)
	{
		value /= 1024;
		unit++;
	}
	QString num = unit == 0 ? QString::number(qRound64(value)) : QString::number(value, 'f', 1);
	return num + ' ' + units[unit];
}

QString formatSpeed(qint64 bytesPerSecond)
{
	return formatSize(bytesPerSecond) + "/s";
}

QString formatEta(qint64 total, qint64 done, qint64 bytesPerSecond)
{
	qint64 remaining = total - done;
	if (bytesPerSecond <= 0 || remaining <= 0)
		return QString::fromUtf8("—");

	qint64 secs = (remaining + bytesPerSecond - 1) / bytesPerSecond;
	if (secs < 60)
		return QString("%1s").arg(secs);
	if (secs < 3600)
		return QString("%1m %2s").arg(secs / 60).arg(secs % 60);
	return QString("%1h %2m").arg(secs / 3600).arg((secs % 3600) / 60);
}

QString formatDuration(double seconds)
{
	qint64 s = qint64(seconds);
	if (s < 60)
		return QString("%1s").arg(s);
	if (s < 3600)
		return QString("%1m %2s").arg(s / 60).arg(s % 60);
	if (s < 86400)
		return QString("%1h %2m").arg(s / 3600).arg((s % 3600) / 60);
	return QString("%1d %2h").arg(s / 86400).arg((s % 86400) / 3600);
}

// Independent of the C library's locale, so the layout is the same in tests and in any terminal
static int cellWidth(char32_t c)
{
	if (c == 0 || (c >= 0x300 && c <= 0x36F) || (c >= 0x200B && c <= 0x200F) || (c >= 0xFE00 && c <= 0xFE0F))
		return 0;
	if (c < 0x1100)
		return 1;
	if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3)
		|| (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60)
		|| (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1F64F) || (c >= 0x1F900 && c <= 0x1F9FF)
		|| (c >= 0x20000 && c <= 0x3FFFD))
		return 2;
	return 1;
}

int displayWidth(const QString& s)
{
	int w = 0;
	const auto ucs = s.toUcs4();
	for (char32_t c : ucs)
		w += cellWidth(c);
	return w;
}

QString elide(const QString& s, int width, bool ascii)
{
	if (width <= 0)
		return QString();
	if (displayWidth(s) <= width)
		return s;

	const QString ell = ascii ? QStringLiteral("~") : QString::fromUtf8("…");
	int budget = width - 1;
	QString out;
	int w = 0;
	for (char32_t c : s.toUcs4())
	{
		int cw = cellWidth(c);
		if (w + cw > budget)
			break;
		out += QString::fromUcs4(&c, 1);
		w += cw;
	}
	return out + ell;
}

QString fit(const QString& s, int width)
{
	if (width <= 0)
		return QString();
	QString out;
	int w = 0;
	for (char32_t c : s.toUcs4())
	{
		int cw = cellWidth(c);
		if (w + cw > width)
			break;
		out += QString::fromUcs4(&c, 1);
		w += cw;
	}
	return out + QString(width - w, ' ');
}

QString fitRight(const QString& s, int width)
{
	int w = displayWidth(s);
	if (w >= width)
		return fit(s, width);
	return QString(width - w, ' ') + s;
}

QString repeat(const QString& s, int n)
{
	QString out;
	for (int i = 0; i < n; i++)
		out += s;
	return out;
}

}
