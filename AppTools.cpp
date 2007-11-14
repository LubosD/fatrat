#include <QWidget>
#include "AppTools.h"
#include "TorrentSearch.h"

static const AppTool m_tools[] = {
		{ "BitTorrent search", TorrentSearch::create },
		{ 0, 0 }
};

const AppTool* getAppTools()
{
	return m_tools;
}
