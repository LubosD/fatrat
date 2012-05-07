#ifndef MHDFILESERVICE_H
#define MHDFILESERVICE_H
#include <QString>
#include <QDir>
#include <magic.h>
#include "MHDService.h"

class MHDFileService : public MHDService
{
public:
	MHDFileService(QString baseDirectory);
	~MHDFileService();
	virtual void handleGet(QString url, MHDConnection& conn);
private:
	QString m_strBaseDirectory;
	magic_t m_magic;
};

#endif

