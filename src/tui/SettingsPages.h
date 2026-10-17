#pragma once
#include "Dialog.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

namespace tui
{
	// One setting shown as one form field. The settings pages are known by the `script` the server reports for them,
	// the same key the web interface uses to pick its page component.
	struct SettingBinding
	{
		enum Kind { Bool, Int, Real, Text, Secret, Choice } kind = Text;
		QString key, label;
		double min = 0, max = 0;     // Int, Real; a max that is not above min means no upper limit
		QString suffix;              // shown after a text field
		QStringList options;         // Choice: the stored value is the index of the option
	};

	struct SettingsPageSpec
	{
		struct Item
		{
			enum Type { Binding, Heading, Note, Spacer } type = Binding;
			SettingBinding binding;
			QString text;
		};
		QString script;
		QVector<Item> items;
		QStringList keys() const;
	};

	// Null for a script this client does not know about
	const SettingsPageSpec* settingsPageSpec(const QString& script);

	// The Extensions page is not a fixed list of settings: the plugins the
	// server has installed decide what it contains. `plugins` is the array
	// GET /plugins returns.
	const char* const ExtensionsScript = "/scripts/settings/extensions.js";
	SettingsPageSpec buildExtensionsSpec(const QJsonArray& plugins);
	// Creates the fields of a page from the values the server returned
	void buildSettingsPanel(const SettingsPageSpec& spec, const QJsonObject& loaded, FormPanel& panel);
	// The values the user changed (only those), ready for PATCH /settings. Returns false, with a message and the key
	// of the offending field, if a field cannot be used.
	bool collectSettings(const SettingsPageSpec& spec, const FormPanel& panel, const QJsonObject& loaded, QJsonObject& changes, QString* error, QString* badKey);

	// The global speed limits of the status bar, in bytes per second (0 = unlimited)
	extern const char* const SettingSpeedDown;
	extern const char* const SettingSpeedUp;
}
