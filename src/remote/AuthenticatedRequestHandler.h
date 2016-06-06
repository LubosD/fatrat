#ifndef AUTHENTICATEDREQUESTHANDLER_H
#define AUTHENTICATEDREQUESTHANDLER_H
#include <QString>
#include <Poco/Net/AbstractHTTPRequestHandler.h>

class AuthenticatedRequestHandler : public Poco::Net::AbstractHTTPRequestHandler
{
public:
	virtual bool authenticate() override;
	static void setPassword(const QString& password);
private:
	static QString m_strPassword;
};

#endif // AUTHENTICATEDREQUESTHANDLER_H
