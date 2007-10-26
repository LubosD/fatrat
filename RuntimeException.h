#ifndef RUNTIMEEXCEPTION_H
#define RUNTIMEEXCEPTION_H
#include <QString>

class RuntimeException
{
public:
	RuntimeException(QString str = QString())
	: m_error(str)
	{
	}
	QString what() const
	{
		return m_error;
	}
private:
	QString m_error;
};

#endif
