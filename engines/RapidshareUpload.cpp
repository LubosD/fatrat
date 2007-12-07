#include "RapidshareUpload.h"

RapidshareUpload::RapidshareUpload()
{
}

RapidshareUpload::~RapidshareUpload()
{
}

int RapidshareUpload::acceptable(QString url)
{
	return 0; // because this class doesn't actually use URLs
}

void RapidshareUpload::init(QString source, QString target)
{
}

void RapidshareUpload::setObject(QString source)
{
}

void RapidshareUpload::changeActive(bool nowActive)
{
}

void RapidshareUpload::setSpeedLimits(int, int up)
{
}

void RapidshareUpload::speeds(int& down, int& up) const
{
}

qulonglong RapidshareUpload::done() const
{
}

void RapidshareUpload::load(const QDomNode& map)
{
}

void RapidshareUpload::save(QDomDocument& doc, QDomNode& map)
{
}

WidgetHostChild* RapidshareUpload::createOptionsWidget(QWidget*)
{
}
