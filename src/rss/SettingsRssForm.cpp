/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "SettingsRssForm.h"

#include <QMap>
#include <QSettings>

#include "RssFeedDlg.h"
#include "RssRegexpDlg.h"
#include "Settings.h"
#include "fatrat.h"

extern QSettings* g_settings;

SettingsRssForm::SettingsRssForm(QWidget* w, QObject* parent)
    : QObject(parent) {
  setupUi(w);

  connect(pushFeedAdd, SIGNAL(clicked()), this, SLOT(feedAdd()));
  connect(pushFeedEdit, SIGNAL(clicked()), this, SLOT(feedEdit()));
  connect(pushFeedDelete, SIGNAL(clicked()), this, SLOT(feedDelete()));

  connect(pushRegexpAdd, SIGNAL(clicked()), this, SLOT(regexpAdd()));
  connect(pushRegexpEdit, SIGNAL(clicked()), this, SLOT(regexpEdit()));
  connect(pushRegexpDelete, SIGNAL(clicked()), this, SLOT(regexpDelete()));

  connect(pushUpdate, SIGNAL(clicked()), RssFetcher::instance(),
          SLOT(refresh()));
}

void SettingsRssForm::load() {
  m_feeds.clear();
  m_regexps.clear();

  listFeeds->clear();
  listRegexps->clear();

  QMap<QString, QString> map;

  RssFetcher::loadRegexps(m_regexps);
  RssFetcher::loadFeeds(m_feeds);

  foreach (RssFeed feed, m_feeds) {
    listFeeds->addItem(feed.name);
    map[feed.url] = feed.name;
  }

  foreach (RssRegexp regexp, m_regexps) {
    QString patt = regexp.regexp.pattern();
    if (!patt.isEmpty())
      listRegexps->addItem(patt + " @ " + map[regexp.source]);
    else
      listRegexps->addItem(map[regexp.source]);
  }

  checkEnable->setChecked(getSettingsValue("rss/enable").toBool());
  spinUpdateInterval->setValue(getSettingsValue("rss/interval").toInt());
}

void SettingsRssForm::accepted() {
  RssFetcher::saveRegexps(m_regexps);
  RssFetcher::saveFeeds(m_feeds);
  g_settings->setValue("rss/enable", checkEnable->isChecked());
  g_settings->setValue("rss/interval", spinUpdateInterval->value());

  applySettings();
}

void SettingsRssForm::applySettings() {
  RssFetcher::instance()->applySettings();
}

void SettingsRssForm::feedAdd() {
  RssFeedDlg dlg(listFeeds->parentWidget());
  if (dlg.exec() == QDialog::Accepted) {
    RssFeed feed;
    feed.name = dlg.m_strName;
    feed.url = dlg.m_strURL;
    m_feeds << feed;

    listFeeds->addItem(feed.name);
  }
}

void SettingsRssForm::feedEdit() {
  int ix;

  if ((ix = listFeeds->currentRow()) < 0) return;

  RssFeedDlg dlg(listFeeds->parentWidget());

  dlg.m_strName = m_feeds[ix].name;
  dlg.m_strURL = m_feeds[ix].url;

  if (dlg.exec() == QDialog::Accepted) {
    if (m_feeds[ix].url != dlg.m_strURL) {
      for (int i = 0; i < m_regexps.size(); i++) {
        if (m_regexps[i].source == m_feeds[ix].url)
          m_regexps[i].source = dlg.m_strURL;
      }
    }

    m_feeds[ix].name = dlg.m_strName;
    m_feeds[ix].url = dlg.m_strURL;

    listFeeds->item(ix)->setText(m_feeds[ix].name);
  }
}

void SettingsRssForm::feedDelete() {
  int ix;

  if ((ix = listFeeds->currentRow()) < 0) return;

  for (int i = 0; i < m_regexps.size(); i++) {
    if (m_regexps[i].source == m_feeds[ix].url) m_regexps.removeAt(i--);
  }

  m_feeds.removeAt(ix);

  delete listFeeds->takeItem(ix);
}

void SettingsRssForm::regexpAdd() {
  RssRegexpDlg dlg(listFeeds->parentWidget());
  dlg.m_feeds = m_feeds;

  if (dlg.exec() == QDialog::Accepted) {
    QString expr = dlg.m_regexp.regexp.pattern();

    if (!expr.isEmpty())
      listRegexps->addItem(expr + " @ " + dlg.m_strFeedName);
    else
      listRegexps->addItem(dlg.m_strFeedName);

    m_regexps << dlg.m_regexp;
  }
}

void SettingsRssForm::regexpEdit() {
  int ix;

  if ((ix = listRegexps->currentRow()) < 0) return;

  RssRegexpDlg dlg(listFeeds->parentWidget());
  dlg.m_feeds = m_feeds;
  dlg.m_regexp = m_regexps[ix];

  if (dlg.exec() == QDialog::Accepted) {
    m_regexps[ix] = dlg.m_regexp;
    QString expr = dlg.m_regexp.regexp.pattern();

    if (!expr.isEmpty())
      listRegexps->item(ix)->setText(expr + " @ " + dlg.m_strFeedName);
    else
      listRegexps->item(ix)->setText(dlg.m_strFeedName);
  }
}

void SettingsRssForm::regexpDelete() {
  int ix;

  if ((ix = listRegexps->currentRow()) < 0) return;
  m_regexps.removeAt(ix);

  delete listRegexps->takeItem(ix);
}
