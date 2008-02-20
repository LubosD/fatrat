#include "RssFetcher.h"

RssFetcher::RssFetcher()
	: m_http(0)
{
	m_timer.start(15*60);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(refresh()));
}

void RssFetcher::refresh()
{
	m_http = new QHttp(this);
}
