#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H
#include <QMap>
#include <QString>

class HttpHandler;

struct HttpRequest
{
    QString method, uri, queryString;
    QMap<QString,QString> headers;
    QMap<QString,QString> getVars, postVars;
};

#endif // HTTPREQUEST_H
