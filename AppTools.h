#ifndef APPTOOLS_H
#define APPTOOLS_H
#include <QWidget>
#include <QString>

struct AppTool
{
	AppTool(QString n, QWidget* (*pfn)()) : strName(n), pfnCreate(pfn)
	{}
	QString strName;
	QWidget* (*pfnCreate)();
};

void initAppTools();
void addAppTool(const AppTool& tool);

#endif
