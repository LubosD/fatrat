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

#ifndef TRANSFERHTTPSERVICE_H
#define TRANSFERHTTPSERVICE_H
#include <QMultiMap>
#include <QString>
#include <QVariant>

class TransferHttpService {
 public:
  class WriteBack {
   public:
    virtual void setContentType(const char* type) = 0;
    virtual void write(const char* data, size_t bytes) = 0;
    virtual void writeNoCopy(void* data, size_t bytes) = 0;
    virtual void writeFail(QString error) = 0;
    virtual void send() = 0;
  };

  // process a HTTP request
  virtual void process(QString method, QMap<QString, QString> args,
                       WriteBack* wb) = 0;

  // location of the script contolling the HTTP details view
  virtual const char* detailsScript() const = 0;

  // properties the script can access via XML-RPC
  virtual QVariantMap properties() const = 0;
};

#endif  // TRANSFERHTTPSERVICE_H
