/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2011 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#ifndef JAVAEXTRACTOR_H
#define JAVAEXTRACTOR_H

#include "config.h"
#include "Transfer.h"
#include "java/JObject.h"
#include "StaticTransferMessage.h"
#include <string>
#include <QRegExp>
#include <QMap>
#include "JavaPersistentVariables.h"

#ifndef WITH_JPLUGINS
#	error This file is not supposed to be included!
#endif

class QNetworkAccessManager;
class QNetworkReply;
class JExtractorPlugin;

class JavaExtractor : public StaticTransferMessage<Transfer>, protected JavaPersistentVariables
{
Q_OBJECT
public:
    JavaExtractor(const char* clsName);
    ~JavaExtractor();

    static Transfer* createInstance(const EngineEntry* e) { return new JavaExtractor(e->shortName); }
    static int acceptable(QString uri, bool, const EngineEntry* e);
    static void globalInit();

    virtual void speeds(int& down, int& up) const { down = up = 0; }
    virtual qulonglong total() const { return 0; }
    virtual qulonglong done() const { return 0; }

    virtual QString object() const { return QString(); }
    virtual void setObject(QString object) {}
    virtual QString myClass() const { return m_strClass; }
    virtual QString name() const;
    virtual void init(QString source, QString target);
    virtual void changeActive(bool nowActive);

    virtual void load(const QDomNode& map);
    virtual void save(QDomDocument& doc, QDomNode& map) const;
protected:
    void finishedExtraction(QList<QString> list);
protected slots:
    void finished(QNetworkReply* reply);
private:
    struct JavaEngine
    {
	    std::string name, shortName;
	    QRegExp regexp;
	    JObject ownAcceptable;
	    QString targetClass;
    };

    static QMap<QString,JavaEngine> m_engines;
private:
    QString m_strClass, m_strUrl, m_strTarget;
    QNetworkAccessManager* m_network;
    JExtractorPlugin* m_plugin;
    QNetworkReply* m_reply;

    friend class JExtractorPlugin;
};

#endif // JAVAEXTRACTOR_H
