/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#include "bencodeparser.h"
#include "metainfo.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QtDebug>

MetaInfo::MetaInfo()
{
	clear();
}

void MetaInfo::clear()
{
	errString = "Unknown error";
	content.clear();
	infoData.clear();
	metaInfoMultiFiles.clear();
	//metaInfoAnnounce.clear();
	metaInfoAnnounceList.clear();
	metaInfoCreationDate = QDateTime();
	metaInfoComment.clear();
	metaInfoCreatedBy.clear();
	metaInfoName.clear();
	metaInfoPieceLength = 0;
	metaInfoSha1Sums.clear();
}

QByteArray MetaInfo::dataContent() const
{
	return content;
}

bool MetaInfo::parse(const QByteArray &data)
{
	clear();
	content = data;

	BencodeParser parser;
	if (!parser.parse(content))
	{
		errString = parser.errorString();
		qDebug() << "BencodeParser error" << errString;
		return false;
	}

	infoData = parser.infoSection();

	QMap<QByteArray, QVariant> dict = parser.dictionary();
	if (!dict.contains("info"))
		return false;

	QMap<QByteArray, QVariant> info = qVariantValue<Dictionary>(dict.value("info"));

	if (info.contains("files"))
	{
		metaInfoFileForm = MultiFileForm;

		QList<QVariant> files = info.value("files").toList();

		for (int i = 0; i < files.size(); ++i)
		{
			QMap<QByteArray, QVariant> file = qVariantValue<Dictionary>(files.at(i));
			QList<QVariant> pathElements = file.value("path").toList();
			QByteArray path;
			foreach (QVariant p, pathElements)
			{
				if (!path.isEmpty())
					path += "/";
				path += p.toByteArray();
			}

			MetaInfoMultiFile multiFile;
			multiFile.length = file.value("length").toLongLong();
			multiFile.path = QString::fromUtf8(path);
			multiFile.md5sum = file.value("md5sum").toByteArray();
			metaInfoMultiFiles << multiFile;
		}

		metaInfoName = QString::fromUtf8(info.value("name").toByteArray());
		qDebug() << "Torrent name" << metaInfoName;
		metaInfoPieceLength = info.value("piece length").toInt();
		QByteArray pieces = info.value("pieces").toByteArray();
		for (int i = 0; i < pieces.size(); i += 20)
			metaInfoSha1Sums << pieces.mid(i, 20);
	}
	else if (info.contains("length"))
	{
		metaInfoFileForm = SingleFileForm;
		metaInfoSingleFile.length = info.value("length").toLongLong();
		metaInfoSingleFile.md5sum = info.value("md5sum").toByteArray();
		metaInfoSingleFile.name = QString::fromUtf8(info.value("name").toByteArray());
		metaInfoSingleFile.pieceLength = info.value("piece length").toInt();

		QByteArray pieces = info.value("pieces").toByteArray();
		for (int i = 0; i < pieces.size(); i += 20)
			metaInfoSingleFile.sha1Sums << pieces.mid(i, 20);
	}

	if (dict.contains("announce-list"))
	{
		// ### unimplemented
		QList<QVariant> trackers = dict.value("announce-list").toList();
		foreach(QVariant v,trackers)
		metaInfoAnnounceList << v.toByteArray();
	}
	else
		metaInfoAnnounceList << QString::fromUtf8(dict.value("announce").toByteArray());

	if (dict.contains("creation date"))
		metaInfoCreationDate.setTime_t(dict.value("creation date").toInt());
	if (dict.contains("comment"))
		metaInfoComment = QString::fromUtf8(dict.value("comment").toByteArray());
	if (dict.contains("created by"))
		metaInfoCreatedBy = QString::fromUtf8(dict.value("created by").toByteArray());

	return true;
}

QByteArray MetaInfo::infoValue() const
{
	return infoData;
}

QString MetaInfo::errorString() const
{
	return errString;
}

MetaInfo::FileForm MetaInfo::fileForm() const
{
	return metaInfoFileForm;
}

//QString MetaInfo::announceUrl() const
//{
//	return metaInfoAnnounce;
//}

QStringList MetaInfo::announceList() const
{
	return metaInfoAnnounceList;
}

QDateTime MetaInfo::creationDate() const
{
	return metaInfoCreationDate;
}

QString MetaInfo::comment() const
{
	return metaInfoComment;
}

QString MetaInfo::createdBy() const
{
	return metaInfoCreatedBy;
}

MetaInfoSingleFile MetaInfo::singleFile() const
{
	return metaInfoSingleFile;
}

QList<MetaInfoMultiFile> MetaInfo::multiFiles() const
{
	return metaInfoMultiFiles;
}

QString MetaInfo::name() const
{
	return metaInfoName;
}

int MetaInfo::pieceLength() const
{
	return metaInfoPieceLength ? metaInfoPieceLength : metaInfoSingleFile.pieceLength;
}

QList<QByteArray> MetaInfo::sha1Sums() const
{
	return metaInfoSha1Sums;
}

qint64 MetaInfo::totalSize() const
{
	if (fileForm() == SingleFileForm)
		return singleFile().length;

	qint64 size = 0;
	foreach (MetaInfoMultiFile file, multiFiles())
	size += file.length;
	return size;
}
