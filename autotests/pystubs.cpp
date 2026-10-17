/* Stand-ins for the parts of FatRat that the Python bridge calls into.
 *
 * The real Settings.cpp drags in the scheduler, the RSS fetcher and the
 * settings-page registry, none of which the bridge needs; this keeps the test
 * binary to the bridge itself.
 */
// Python headers before Qt headers; see src/python/PyRef.h.
#include "PyBindings.h"

#include <QMap>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariant>

#include "Settings.h"
#include "Transfer.h"
#include "captcha/Captcha.h"

QSettings* g_settings = 0;
QVector<SettingsItem> g_settingsPages;

static QMap<QString, QVariant>& store()
{
	static QMap<QString, QVariant> map;
	return map;
}

static QMap<QString, QList<QMap<QString, QVariant> > >& arrayStore()
{
	static QMap<QString, QList<QMap<QString, QVariant> > > map;
	return map;
}

QVariant getSettingsDefault(QString)
{
	return QVariant();
}

QVariant getSettingsValue(QString id, QVariant def)
{
	return store().contains(id) ? store()[id] : def;
}

void setSettingsValue(QString id, QVariant value)
{
	store()[id] = value;
}

QList<QMap<QString, QVariant> > getSettingsArray(QString id)
{
	return arrayStore().value(id);
}

void setSettingsArray(QString id, QList<QMap<QString, QVariant> >& value)
{
	arrayStore()[id] = value;
}


/* Transfer: the bridge calls this on a transfer it was given, and the test
 * gives it none -- but the symbol still has to exist. (Logging goes through
 * Logger, which the test links for real.)
 */
void Transfer::setState(Transfer::State)
{
}

/* Captcha: answers instantly, so the captcha path is testable without a UI.
 * A URL containing "unsolvable" stands for the user giving up.
 */
static int s_nextCaptcha = 1;
static QMap<QString, bool> s_abortedCaptchas;

int Captcha::processCaptcha(QString url, CallbackFn fn)
{
	int id = s_nextCaptcha++;

	s_abortedCaptchas[url] = false;

	QTimer::singleShot(0, [url, fn]()
	{
		if (s_abortedCaptchas.value(url))
			return;

		fn(url, url.contains("unsolvable") ? QString() : QString("XYZZY"));
	});

	return id;
}

void Captcha::abortCaptcha(int)
{
}

void Captcha::abortCaptcha(QString url)
{
	s_abortedCaptchas[url] = true;
}


/* The transfer-scoped bindings (set_message, vars, start_download, ...) live
 * with the download engine, which this test binary does not build: they need
 * a real transfer, and the engine is exercised by running FatRat itself.
 */
void registerTransferBindings(PyPluginClass&)
{
}
