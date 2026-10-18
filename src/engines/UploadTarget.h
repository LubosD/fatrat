#ifndef UPLOADTARGET_H
#define UPLOADTARGET_H
#include <QUrl>
#include <QJsonObject>

// Composes the upload target URL for a partial options update.
// A "target" that carries user info uses it; otherwise the current credentials
// are kept. Explicit "username"/"password" keys always win.
inline QUrl composeUploadTarget(const QUrl& current, const QJsonObject& values)
{
	QUrl url = current;

	if (values.contains("target"))
	{
		QUrl newUrl(values.value("target").toString().trimmed());
		if (newUrl.userInfo().isEmpty())
		{
			newUrl.setUserName(current.userName());
			newUrl.setPassword(current.password());
		}
		url = newUrl;
	}
	if (values.contains("username"))
		url.setUserName(values.value("username").toString());
	if (values.contains("password"))
		url.setPassword(values.value("password").toString());
	return url;
}

#endif
