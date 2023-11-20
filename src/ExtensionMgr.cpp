#include "ExtensionMgr.h"

#include "fatrat.h"
#include "java/JVM.h"

static const QLatin1String UPDATE_URL =
    QLatin1String("http://fatrat.dolezel.info/update/plugins/");

ExtensionMgr::ExtensionMgr() {
  m_installedPackages = JVM::instance()->getPackageVersions();

  connect(&m_network, SIGNAL(finished(QNetworkReply*)), this,
          SLOT(dataLoaded(QNetworkReply*)));
}

void ExtensionMgr::loadFromServer() {
  m_network.get(QNetworkRequest(QUrl(UPDATE_URL + "Index?version=" + VERSION)));
}

void ExtensionMgr::dataLoaded(QNetworkReply* req) {
  req->deleteLater();

  if (req->error() != QNetworkReply::NoError) {
    emit loadFailed();
    return;
  }

  QList<PackageInfo> pkgs;

  while (!req->atEnd()) {
    QString line = req->readLine().trimmed();
    if (line.isEmpty()) continue;
    QStringList fields = line.split('\t');
    if (fields.size() < 3) continue;

    PackageInfo info;
    info.name = fields[0];
    info.latestVersion = fields[1];
    info.desc = fields[2];

    pkgs << info;
  }

  for (QMap<QString, QString>::iterator it = m_installedPackages.begin();
       it != m_installedPackages.end(); it++) {
    QString name = it.key();
    name.resize(name.length() - 4);

    for (int i = 0; i < pkgs.size(); i++) {
      if (pkgs[i].name.compare(name, Qt::CaseInsensitive) == 0) {
        pkgs[i].installedVersion = it.value();
        break;
      }
    }
  }

  m_packages = pkgs;
  emit loaded();
}

QList<ExtensionMgr::PackageInfo> ExtensionMgr::getPackages() {
  return m_packages;
}
