/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef METALINKDOWNLOAD_H
#define METALINKDOWNLOAD_H
#include "Transfer.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;

class MetalinkDownload : public Transfer
{
Q_OBJECT
public:
	MetalinkDownload();
	virtual ~MetalinkDownload();

	static void globalInit();
	static int acceptable(QString url, bool);

	virtual void changeActive(bool nowActive);
	virtual void init(QString source, QString target);
	virtual void setObject(QString object) { m_strTarget = object; }
	virtual QString object() const { return m_strTarget; }
	virtual QString myClass() const { return "MetalinkDownload"; }
	virtual QString name() const;
	virtual QString message() const { return m_strMessage; }
	virtual void speeds(int& down, int& up) const { down = up = 0; }
	virtual qulonglong total() const { return 0; }
	virtual qulonglong done() const { return 0; }
	virtual void load(const QDomNode& map);
	virtual void save(QDomDocument& doc, QDomNode& map) const;
	virtual QString remoteURI() const;

	static Transfer* createInstance() { return new MetalinkDownload; }
private slots:
	void done(QNetworkReply* reply);
	void networkReadyRead();
	void processMetalink(QString file);
protected:

	struct Link
	{
		Link(QString _url, int _priority, bool _isTorrent) : url(_url), isTorrent(_isTorrent), priority(_priority)
		{
		}

		QString url;
		bool isTorrent;
		int priority;

		bool operator<(const Link& that) const
		{
			return priority < that.priority;
		}
	};
	struct MetaFile
	{
		QString name;
		QList<Link> urls;
		qlonglong fileSize;
		QString comment;
		QString hashMD5, hashSHA1;

		bool hasHTTP, hasTorrent;
	};

private:
	QString m_strMessage, m_strSource, m_strTarget;
	QNetworkAccessManager* m_network;
	QTemporaryFile* m_file;
	QNetworkReply* m_reply;
};

#endif // METALINKDOWNLOAD_H
