#ifndef SETTINGSSTRING_H
#define SETTINGSSTRING_H
#include <QVariant>
#include <QMetaType>
#include <QStringList>

// QSettings reads an unquoted "a,b" from an INI file as a QStringList, whose toString() is empty.
// For values that are really strings (a hand-edited password, a path) join the parts back with commas.
// *unreadable is set when the value exists but is of a type that cannot be turned into text, so that
// callers guarding something with it (a password) can refuse access rather than treat it as "not set".
inline QString settingToString(const QVariant& v, bool* unreadable = nullptr)
{
	if (unreadable)
		*unreadable = false;

	if (v.metaType() == QMetaType::fromType<QStringList>())
		return v.toStringList().join(',');

	const QString s = v.toString();
	if (unreadable && s.isEmpty() && v.isValid() && !v.isNull()
		&& v.metaType() != QMetaType::fromType<QString>()
		&& v.metaType() != QMetaType::fromType<QByteArray>())
	{
		*unreadable = true;
	}
	return s;
}

#endif
