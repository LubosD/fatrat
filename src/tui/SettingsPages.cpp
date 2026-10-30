#include "SettingsPages.h"
#include "Format.h"
#include <QLocale>

namespace tui
{

namespace
{
	using Item = SettingsPageSpec::Item;
	using B = SettingBinding;

	Item makeItem(B::Kind kind, const QString& key, const QString& label, const QStringList& options = QStringList())
	{
		Item i;
		i.binding.kind = kind;
		i.binding.key = key;
		i.binding.label = label;
		i.binding.options = options;
		return i;
	}
	Item makeHeading(const QString& t) { Item i; i.type = Item::Heading; i.text = t; return i; }
	Item makeNote(const QString& t) { Item i; i.type = Item::Note; i.text = t; return i; }
	Item makeSpacer() { Item i; i.type = Item::Spacer; return i; }

	// A plugin's config items, flattened. `group` is prefixed to the labels
	// so a field says which plugin it belongs to.
	void appendConfig(const QJsonArray& config, const QString& group, QVector<Item>& into)
	{
		for (const QJsonValue& v : config)
		{
			const QJsonObject item = v.toObject();
			const QString type = item["type"].toString();
			const QString label = item["title"].toString();
			const QString key = item["key"].toString();

			if (type == "group")
			{
				appendConfig(item["items"].toArray(), group, into);
				continue;
			}

			if (key.isEmpty())
				continue;

			if (type == "text")
				into << makeItem(B::Text, key, label);
			else if (type == "password")
				into << makeItem(B::Secret, key, label);
			else if (type == "checkbox")
				into << makeItem(B::Bool, key, label);
			else
			{
				// Choice stores an index here but a value in the
				// settings, and `order` has no field type at all:
				// say so instead of corrupting the value.
				into << makeNote(QObject::tr("%1: edit in the desktop or web interface").arg(label.isEmpty() ? key : label));
			}
		}
	}
}

SettingsPageSpec buildExtensionsSpec(const QJsonArray& plugins)
{
	SettingsPageSpec spec;

	spec.script = ExtensionsScript;

	spec.items << makeNote(QObject::tr("Press F4 to add repositories and install plugins."));
	spec.items << makeSpacer();

	if (plugins.isEmpty())
	{
		spec.items << makeNote(QObject::tr("No plugins are installed."));
		return spec;
	}

	for (const QJsonValue& v : plugins)
	{
		const QJsonObject plugin = v.toObject();
		const QString name = plugin["name"].toString();

		if (!plugin["valid"].toBool())
		{
			spec.items << makeHeading(name);
			spec.items << makeNote(plugin["error"].toString());
			spec.items << makeSpacer();
			continue;
		}

		QString title = name;

		if (!plugin["version"].toString().isEmpty())
			title += " " + plugin["version"].toString();

		spec.items << makeHeading(title);

		if (!plugin["description"].toString().isEmpty())
			spec.items << makeNote(plugin["description"].toString());

		const QString enabledKey = plugin["enabledKey"].toString();

		if (!enabledKey.isEmpty())
		{
			spec.items << makeItem(B::Bool, enabledKey, QObject::tr("Enabled"));
		}

		appendConfig(plugin["config"].toArray(), name, spec.items);
		spec.items << makeSpacer();
	}

	return spec;
}

QStringList SettingsPageSpec::keys() const
{
	QStringList k;
	for (const Item& i : items)
		if (i.type == Item::Binding)
			k << i.binding.key;
	return k;
}

// ------------------------------------------------------------------ the pages
// The keys, labels and limits are those of webui/src/app/features/settings/*.html

namespace
{
	using Item = SettingsPageSpec::Item;
	using B = SettingBinding;

	Item item(B::Kind kind, const QString& key, const QString& label, double min = 0, double max = 0, const QString& suffix = QString(), const QStringList& options = QStringList())
	{
		Item i;
		i.binding.kind = kind;
		i.binding.key = key;
		i.binding.label = label;
		i.binding.min = min;
		i.binding.max = max;
		i.binding.suffix = suffix;
		i.binding.options = options;
		return i;
	}
	Item heading(const QString& t) { Item i; i.type = Item::Heading; i.text = t; return i; }
	Item note(const QString& t) { Item i; i.type = Item::Note; i.text = t; return i; }
	Item spacer() { Item i; i.type = Item::Spacer; return i; }

	QVector<SettingsPageSpec> buildSpecs()
	{
		const QString portRange = QStringLiteral("1024-65535");
		const QStringList mode = { QObject::tr("Disabled"), QObject::tr("Enabled"), QObject::tr("Permit only encrypted") };

		SettingsPageSpec bt;
		bt.script = "/scripts/settings/bittorrent.js";
		bt.items = {
			heading(QObject::tr("Main")),
			item(B::Int, "torrent/listen_start", QObject::tr("Listen port (from)"), 1024, 65535, portRange),
			item(B::Int, "torrent/listen_end", QObject::tr("Listen port (to)"), 1024, 65535, portRange),
			item(B::Real, "torrent/maxratio", QObject::tr("Seed up to ratio"), 0, 0, QObject::tr("default value")),
			item(B::Int, "torrent/maxconnections", QObject::tr("Max connections (global)"), 10),
			item(B::Int, "torrent/maxconnections_loc", QObject::tr("Max connections (per torrent)"), 10),
			item(B::Int, "torrent/maxuploads", QObject::tr("Max uploads (global)"), 1),
			item(B::Int, "torrent/maxuploads_loc", QObject::tr("Max uploads (per torrent)"), 1),
			item(B::Int, "torrent/maxfiles", QObject::tr("Max open files"), 5),
			item(B::Choice, "torrent/allocation", QObject::tr("File allocation mode"), 0, 0, QString(), { QObject::tr("Full"), QObject::tr("Sparse"), QObject::tr("Compact") }),
			item(B::Text, "torrent/external_ip", QObject::tr("External IP")),
			item(B::Bool, "torrent/dht", QObject::tr("Distributed tracker (DHT)")),
			item(B::Bool, "torrent/pex", QObject::tr("uTorrent Peer Exchange")),
			item(B::Bool, "torrent/mapping_lsd", QObject::tr("Local Service Discovery")),
			spacer(),
			heading(QObject::tr("Encryption")),
			item(B::Choice, "torrent/enc_incoming", QObject::tr("Incoming connections"), 0, 0, QString(), mode),
			item(B::Choice, "torrent/enc_outgoing", QObject::tr("Outgoing connections"), 0, 0, QString(), mode),
			item(B::Choice, "torrent/enc_level", QObject::tr("Encryption levels"), 0, 0, QString(), { QObject::tr("Plaintext"), QObject::tr("RC4"), QObject::tr("Both") }),
			item(B::Bool, "torrent/enc_rc4_prefer", QObject::tr("Prefer RC4")),
			spacer(),
			heading(QObject::tr("Port mapping")),
			item(B::Bool, "torrent/mapping_upnp", QObject::tr("UPnP")),
			item(B::Bool, "torrent/mapping_natpmp", QObject::tr("NAT-PMP")),
		};

		SettingsPageSpec rss;
		rss.script = "/scripts/settings/rss.js";
		rss.items = {
			item(B::Bool, "rss/enable", QObject::tr("Check the feeds")),
			item(B::Int, "rss/interval", QObject::tr("Check every"), 1, 0, QObject::tr("minutes")),
			spacer(),
			note(QObject::tr("The feeds and the rules are edited on the RSS tab (key 2). Changes there are saved at once, not with this dialog.")),
		};

		SettingsPageSpec web;
		web.script = "/scripts/settings/webinterface.js";
		web.items = {
			item(B::Bool, "remote/enable", QObject::tr("Remote web control")),
			item(B::Int, "remote/port", QObject::tr("Port"), 1024, 65535, portRange),
			item(B::Secret, "remote/password", QObject::tr("Password")),
			note(QObject::tr("Any user name works; only the password is checked.")),
			spacer(),
			item(B::Bool, "remote/ssl", QObject::tr("Use HTTPS")),
			item(B::Text, "remote/ssl_pem", QObject::tr("PEM file")),
			note(QObject::tr("After toggling HTTPS or changing the certificate, clients have to reconnect and accept the new certificate.")),
		};
		return { bt, rss, web };
	}

	// ------------------------------------------------------ loaded values (lenient: settings may come back as text)

	bool loadedBool(const QJsonValue& v)
	{
		if (v.isBool())
			return v.toBool();
		if (v.isString())
		{
			const QString s = v.toString().trimmed().toLower();
			return s == "true" || s == "1" || s == "yes" || s == "on";
		}
		return v.toDouble() != 0;
	}

	qlonglong loadedInt(const QJsonValue& v)
	{
		if (v.isString())
			return v.toString().trimmed().toLongLong();
		return qlonglong(v.toDouble());
	}

	double loadedReal(const QJsonValue& v)
	{
		return v.isString() ? QLocale::c().toDouble(v.toString().trimmed()) : v.toDouble();
	}

	QString realText(double d)
	{
		return QLocale::c().toString(d, 'g', 10);
	}
}

const SettingsPageSpec* settingsPageSpec(const QString& script)
{
	static const QVector<SettingsPageSpec> specs = buildSpecs();
	for (const SettingsPageSpec& s : specs)
		if (s.script == script)
			return &s;
	return nullptr;
}

void buildSettingsPanel(const SettingsPageSpec& spec, const QJsonObject& loaded, FormPanel& panel)
{
	for (const SettingsPageSpec::Item& it : spec.items)
	{
		switch (it.type)
		{
			case SettingsPageSpec::Item::Heading: panel.addLabel(it.text.toUpper()); continue;
			case SettingsPageSpec::Item::Note: panel.addLabel(it.text); continue;
			case SettingsPageSpec::Item::Spacer: panel.addSpacer(); continue;
			case SettingsPageSpec::Item::Binding: break;
		}
		const SettingBinding& b = it.binding;
		const QJsonValue v = loaded.value(b.key);
		switch (b.kind)
		{
			case SettingBinding::Bool:
				panel.addCheck(b.key, b.label, loadedBool(v));
				break;
			case SettingBinding::Int:
				panel.addText(b.key, b.label, QString::number(loadedInt(v)), b.suffix);
				break;
			case SettingBinding::Real:
				panel.addText(b.key, b.label, realText(loadedReal(v)), b.suffix);
				break;
			case SettingBinding::Text:
				panel.addText(b.key, b.label, v.isString() ? v.toString() : QString(), b.suffix);
				break;
			case SettingBinding::Secret:
				panel.addText(b.key, b.label, v.isString() ? v.toString() : QString(), b.suffix).secret = true;
				break;
			case SettingBinding::Choice:
			{
				QStringList values;
				for (int i = 0; i < b.options.size(); i++)
					values << QString::number(i);
				const qlonglong n = loadedInt(v);
				panel.addSelect(b.key, b.label, b.options, values, n >= 0 && n < b.options.size() ? int(n) : 0);
				break;
			}
		}
	}
}

bool collectSettings(const SettingsPageSpec& spec, const FormPanel& panel, const QJsonObject& loaded, QJsonObject& changes, QString* error, QString* badKey)
{
	auto fail = [&](const SettingBinding& b, const QString& msg) {
		if (error)
			*error = b.label + ": " + msg;
		if (badKey)
			*badKey = b.key;
		return false;
	};

	for (const SettingsPageSpec::Item& it : spec.items)
	{
		if (it.type != SettingsPageSpec::Item::Binding)
			continue;
		const SettingBinding& b = it.binding;
		const QJsonValue old = loaded.value(b.key);

		switch (b.kind)
		{
			case SettingBinding::Bool:
				if (panel.checked(b.key) != loadedBool(old))
					changes[b.key] = panel.checked(b.key);
				break;
			case SettingBinding::Int:
			{
				bool ok = false;
				const qlonglong n = panel.text(b.key).trimmed().toLongLong(&ok);
				if (!ok)
					return fail(b, QObject::tr("must be a whole number"));
				if (n < b.min)
					return fail(b, QObject::tr("must be at least %1").arg(qlonglong(b.min)));
				if (b.max > b.min && n > b.max)
					return fail(b, QObject::tr("must be at most %1").arg(qlonglong(b.max)));
				if (n != loadedInt(old))
					changes[b.key] = qint64(n);
				break;
			}
			case SettingBinding::Real:
			{
				bool ok = false;
				const double d = QLocale::c().toDouble(panel.text(b.key).trimmed(), &ok);
				if (!ok)
					return fail(b, QObject::tr("must be a number"));
				if (d < b.min)
					return fail(b, QObject::tr("must be at least %1").arg(realText(b.min)));
				if (d != loadedReal(old))
					changes[b.key] = d;
				break;
			}
			case SettingBinding::Text:
			case SettingBinding::Secret:
				if (panel.text(b.key) != (old.isString() ? old.toString() : QString()))
					changes[b.key] = panel.text(b.key);
				break;
			case SettingBinding::Choice:
			{
				const qlonglong n = loadedInt(old);
				const int was = n >= 0 && n < b.options.size() ? int(n) : 0;
				if (panel.index(b.key) != was)
					changes[b.key] = panel.index(b.key);
				break;
			}
		}
	}
	return true;
}

}
