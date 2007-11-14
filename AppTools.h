#ifndef APPTOOLS_H
#define APPTOOLS_H

struct AppTool
{
	const char* pszName;
	QWidget* (*pfnCreate)();
};

const AppTool* getAppTools();

#endif
