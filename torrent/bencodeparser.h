/****************************************************************************
**
** Copyright (C) 2004-2006 Trolltech ASA
** Copyright (C) 2007 Lubos Dolezel
**
****************************************************************************/

#ifndef BENCODEPARSER_H
#define BENCODEPARSER_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVariant>

template<typename T> class QList;

typedef QMap<QByteArray,QVariant> Dictionary;
Q_DECLARE_METATYPE(Dictionary)

class BencodeParser
{
public:
    BencodeParser();
    
    bool parse(const QByteArray &content);
    QString errorString() const;

    QMap<QByteArray, QVariant> dictionary() const;
    QByteArray infoSection() const;

private:
    bool getByteString(QByteArray *byteString);
    bool getInteger(qint64 *integer);
    bool getList(QList<QVariant> *list);
    bool getDictionary(QMap<QByteArray, QVariant> *dictionary);

    QMap<QByteArray, QVariant> dictionaryValue;

    QString errString;
    QByteArray content;
    int index;

    int infoStart;
    int infoLength;
};

#endif
