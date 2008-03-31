#include "TorrentOptsWidget.h"
#include <QHeaderView>

TorrentOptsWidget::TorrentOptsWidget(QWidget* me, TorrentDownload* parent)
	: m_download(parent), m_bUpdating(false)
{
	setupUi(me);
	
	QTreeWidgetItem* hdr = treeFiles->headerItem();
	hdr->setText(0, tr("Name"));
	hdr->setText(1, tr("Size"));
	
	connect(pushAddUrlSeed, SIGNAL(clicked()), this, SLOT(addUrlSeed()));
	connect(pushTrackerAdd, SIGNAL(clicked()), this, SLOT(addTracker()));
	connect(pushTrackerRemove, SIGNAL(clicked()), this, SLOT(removeTracker()));
	
	connect(treeFiles, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(fileItemChanged(QTreeWidgetItem*,int)));
}

void TorrentOptsWidget::startInvalid()
{
	stackedWidget->setCurrentIndex(0);
	m_timer.start(1000);
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(handleInvalid()));
	handleInvalid();
}

void TorrentOptsWidget::handleInvalid()
{
	if(m_download->m_handle.is_valid())
	{
		m_timer.stop();
		load();
		stackedWidget->setCurrentIndex(1);
	}
	else
	{
		if(m_download->state() == Transfer::Failed)
		{
			labelStatus->setText(tr("The .torrent file cannot be downloaded or is invalid."));
		}
		else
		{
			labelStatus->setText(tr("The .torrent is being downloaded, please wait."));
		}
	}
}

void TorrentOptsWidget::load()
{
	if(!m_download->m_handle.is_valid())
	{
		startInvalid();
		return;
	}
	
	doubleSeed->setValue(m_download->m_seedLimitRatio);
	checkSeedRatio->setChecked(m_download->m_seedLimitRatio > 0.0);
	
	lineSeed->setText(QString::number(m_download->m_seedLimitUpload));
	checkSeedUpload->setChecked(m_download->m_seedLimitUpload > 0);
	
	QHeaderView* hdr = treeFiles->header();
	hdr->resizeSection(0, 350);
	
	for(libtorrent::torrent_info::file_iterator it = m_download->m_info->begin_files();
		it != m_download->m_info->end_files();
		it++)
	{
		QStringList elems = QString::fromUtf8(it->path.string().c_str()).split('/');
		//QString name = elems.takeLast();
		
		QTreeWidgetItem* item = 0;
		
		for(int x=0;x<elems.size();x++)
		{
			if(item != 0)
			{
				bool bFound = false;
				for(int i=0;i<item->childCount();i++)
				{
					QTreeWidgetItem* c = item->child(i);
					if(c->text(0) == elems[x])
					{
						bFound = true;
						item = c;
					}
				}
				
				if(!bFound)
					item = new QTreeWidgetItem(item, QStringList( elems[x] ));
			}
			else
			{
				bool bFound = false;
				for(int i=0;i<treeFiles->topLevelItemCount();i++)
				{
					QTreeWidgetItem* c = treeFiles->topLevelItem(i);
					if(c->text(0) == elems[x])
					{
						bFound = true;
						item = c;
					}
				}
				
				if(!bFound)
					item = new QTreeWidgetItem(treeFiles, QStringList( elems[x] ));
			}
		}
		
		// fill in info
		item->setText(1, formatSize(it->size));
		item->setData(1, Qt::UserRole, qint64(it->size));
		m_files << item;
	}
	
	for(int i=0;i<m_files.size();i++)
	{
		bool download = true;
		if(size_t(i) < m_download->m_vecPriorities.size())
			download = m_download->m_vecPriorities[i] >= 1;
		
		m_files[i]->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
		m_files[i]->setCheckState(0, download ? Qt::Checked : Qt::Unchecked);
	}
	
	recursiveUpdateDown(treeFiles->invisibleRootItem());
	treeFiles->expandAll();
	
	std::set<std::string> seeds = m_download->m_handle.url_seeds();
	for(std::set<std::string>::iterator it=seeds.begin(); it != seeds.end(); it++)
		listUrlSeeds->addItem(QString::fromUtf8(it->c_str()));
	
	m_trackers = m_download->m_handle.trackers();
	for(size_t i=0;i<m_trackers.size();i++)
		listTrackers->addItem(QString::fromUtf8(m_trackers[i].url.c_str()));
}

void TorrentOptsWidget::accepted()
{
	if(!m_download->m_handle.is_valid())
		return;
	
	for(int i=0;i<m_files.size();i++)
	{
		bool yes = m_files[i]->checkState(0) == Qt::Checked;
		int& prio = m_download->m_vecPriorities[i];
		
		if(yes && !prio)
			prio = 1;
		else if(!yes && prio)
			prio = 0;
	}
	m_download->m_handle.prioritize_files(m_download->m_vecPriorities);
	
	std::vector<std::string> seeds = m_download->m_info->url_seeds();
	foreach(QString url, m_seeds)
	{
		if(std::find(seeds.begin(), seeds.end(), url.toStdString()) == seeds.end())
			m_download->m_handle.add_url_seed(url.toStdString());
	}
	for(size_t i=0;i<seeds.size();i++)
	{
		QString url = QString::fromUtf8(seeds[i].c_str());
		if(!m_seeds.contains(url))
			m_download->m_handle.remove_url_seed(seeds[i]);
	}
	
	m_download->m_handle.replace_trackers(m_trackers);
	
	m_download->m_seedLimitRatio = (checkSeedRatio->isChecked()) ? doubleSeed->value() : 0.0;
	m_download->m_seedLimitUpload = (checkSeedUpload->isChecked()) ? lineSeed->text().toInt() : 0;
}

void TorrentOptsWidget::addUrlSeed()
{
	QString url = lineUrl->text();
	if(url.startsWith("http://"))
	{
		m_seeds << url;
		listUrlSeeds->addItem(url);
		lineUrl->clear();
	}
}

void TorrentOptsWidget::addTracker()
{
	QString url = lineTracker->text();
	
	if(url.startsWith("http://") || url.startsWith("udp://"))
	{
		m_trackers.push_back( libtorrent::announce_entry ( url.toStdString() ) );
		listTrackers->addItem(url);
		lineTracker->clear();
	}
}

void TorrentOptsWidget::removeTracker()
{
	int cur = listTrackers->currentRow();
	if(cur != -1)
	{
		delete listTrackers->takeItem(cur);
		m_trackers.erase(m_trackers.begin() + cur);
	}
}

void TorrentOptsWidget::fileItemChanged(QTreeWidgetItem* item, int column)
{
	if(column != 0 || m_bUpdating)
		return;
	
	m_bUpdating = true;
	
	if(item->childCount())
	{
		// directory
		recursiveCheck(item, item->checkState(0));
	}
	
	if(QTreeWidgetItem* parent = item->parent())
		recursiveUpdate(parent);
	
	m_bUpdating = false;
}

void TorrentOptsWidget::recursiveUpdate(QTreeWidgetItem* item)
{
	int yes = 0, no = 0;
	int total = item->childCount();
	
	for(int i=0;i<total;i++)
	{
		int state = item->child(i)->checkState(0);
		if(state == Qt::Checked)
			yes++;
		else if(state == Qt::Unchecked)
			no++;
	}
	
	if(yes == total)
		item->setCheckState(0, Qt::Checked);
	else if(no == total)
		item->setCheckState(0, Qt::Unchecked);
	else
		item->setCheckState(0, Qt::PartiallyChecked);
	
	if(QTreeWidgetItem* parent = item->parent())
		recursiveUpdate(parent);
}

qint64 TorrentOptsWidget::recursiveUpdateDown(QTreeWidgetItem* item)
{
	int yes = 0, no = 0;
	int total = item->childCount();
	qint64 size = 0;
	
	for(int i=0;i<total;i++)
	{
		QTreeWidgetItem* child = item->child(i);
		
		if(child->childCount())
			size += recursiveUpdateDown(child);
		
		int state = child->checkState(0);
		if(state == Qt::Checked)
			yes++;
		else if(state == Qt::Unchecked)
			no++;
		
		size += child->data(1, Qt::UserRole).toLongLong();
	}
	
	if(yes == total)
		item->setCheckState(0, Qt::Checked);
	else if(no == total)
		item->setCheckState(0, Qt::Unchecked);
	else
		item->setCheckState(0, Qt::PartiallyChecked);
	
	item->setText(1, formatSize(size));
	
	return size;
}

void TorrentOptsWidget::recursiveCheck(QTreeWidgetItem* item, Qt::CheckState state)
{
	item->setCheckState(0, state);
	for(int i=0;i<item->childCount();i++)
		recursiveCheck(item->child(i), state);
}

