/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2026 Lubos Dolezel <lubos a dolezel.info>

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

#include "PluginUpdateChecker.h"

#include <QRunnable>
#include <QThreadPool>
#include <QtDebug>

#include "Logger.h"
#include "PluginRepository.h"
#include "Settings.h"

// Once a day is plenty: plugin repositories are not news feeds.
static const int CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;

namespace {

class CheckTask : public QRunnable
{
public:
	CheckTask(PluginUpdateChecker* owner) : m_owner(owner)
	{
		setAutoDelete(true);
	}

	virtual void run()
	{
		QString error;

		if (!PluginRepository::refresh(QString(), &error))
			qDebug() << "Plugin update check failed:" << error;

		int count = PluginRepository::updates().size();

		QMetaObject::invokeMethod(m_owner, "finished", Qt::QueuedConnection, Q_ARG(int, count));
	}
private:
	PluginUpdateChecker* m_owner;
};

} // namespace

PluginUpdateChecker::PluginUpdateChecker(QObject* parent)
	: QObject(parent), m_bRunning(false)
{
	m_timer.setInterval(CHECK_INTERVAL_MS);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(checkNow()));
}

void PluginUpdateChecker::applySettings()
{
	bool enabled = getSettingsValue(QLatin1String("python/check_updates")).toBool();

	if (enabled && !m_timer.isActive())
		m_timer.start();
	else if (!enabled && m_timer.isActive())
		m_timer.stop();
}

void PluginUpdateChecker::checkNow()
{
	if (m_bRunning)
		return;
	if (PluginRepository::repositories().isEmpty())
		return;

	m_bRunning = true;

	QThreadPool::globalInstance()->start(new CheckTask(this));
}

void PluginUpdateChecker::finished(int count)
{
	m_bRunning = false;

	if (count > 0)
	{
		Logger::global()->enterLogMessage(QLatin1String("Python"),
			tr("%1 installed plugins have a newer version available").arg(count));
	}

	emit updatesAvailable(count);
}
