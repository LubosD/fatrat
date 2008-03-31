#ifndef APPTOOLS_H
#define APPTOOLS_H

struct AppTool
{
	AppTool(QString n, QWidget* (*pfn)()) : strName(n), pfnCreate(pfn)
	{}
	QString strName;
	QWidget* (*pfnCreate)();
};

void initAppTools();

#endif
