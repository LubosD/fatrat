#ifndef _WIDGETHOSTCHILD_H
#define _WIDGETHOSTCHILD_H

class WidgetHostChild
{
public:
	virtual ~WidgetHostChild() {}
	virtual void load() {}
	virtual bool accept() { return true; }
	virtual void accepted() {}
};

#endif

