#include "AuthenticatedRequestHandler.h"
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

QString AuthenticatedRequestHandler::m_strPassword;

bool AuthenticatedRequestHandler::authenticate()
{
	std::string scheme, authInfo;

	if (m_strPassword.isEmpty())
		return true;
	if (!request().hasCredentials())
	{
		response().requireAuthentication("FatRat");
		return false;
	}

	request().getCredentials(scheme, authInfo);
	if (scheme != "Basic")
	{
		response().requireAuthentication("FatRat");
		return false;
	}

	if (Poco::Net::HTTPBasicCredentials(authInfo).getPassword() != m_strPassword.toStdString())
	{
		response().requireAuthentication("FatRat");
		return false;
	}

	return true;
}

void AuthenticatedRequestHandler::setPassword(const QString& password)
{
	m_strPassword = password;
}
