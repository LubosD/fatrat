#include "ServerManager.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tui
{

QString ServerManager::defaultSocketPath()
{
	QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
	if (dir.isEmpty() || !QFileInfo(dir).isDir())
		dir = QString("/tmp/fatrat-%1").arg(getuid());
	return dir + "/fatrat/rest.sock";
}

QString ServerManager::defaultLogPath()
{
	QString base = qEnvironmentVariable("XDG_STATE_HOME");
	if (base.isEmpty())
		base = QDir::homePath() + "/.local/state";
	return base + "/fatrat/tui-server.log";
}

QString ServerManager::findServerBinary(const QString& hint)
{
	if (!hint.isEmpty())
		return QFileInfo(hint).isExecutable() ? hint : QString();

	const QString dir = QCoreApplication::applicationDirPath();
	for (const char* name : { "fatrat-nogui", "fatrat" })
	{
		const QString p = dir + "/" + name;
		if (QFileInfo(p).isExecutable() && !QFileInfo(p).isDir())
			return p;
	}
	for (const char* name : { "fatrat-nogui", "fatrat" })
	{
		const QString p = QStandardPaths::findExecutable(name);
		if (!p.isEmpty())
			return p;
	}
	return QString();
}

bool ServerManager::probe(const QString& socketPath, int timeoutMs)
{
	QLocalSocket s;
	s.connectToServer(socketPath);
	const bool ok = s.waitForConnected(timeoutMs);
	s.abort();
	return ok;
}

ServerManager::ServerManager(const Options& o, QObject* parent) : QObject(parent), m_opt(o)
{
	if (m_opt.socketPath.isEmpty())
		m_opt.socketPath = defaultSocketPath();
	if (m_opt.logPath.isEmpty())
		m_opt.logPath = defaultLogPath();
	m_timer.setInterval(200);
	connect(&m_timer, &QTimer::timeout, this, &ServerManager::check);
}

QString ServerManager::lastLogLines(int n) const
{
	QFile f(m_opt.logPath);
	if (!f.open(QIODevice::ReadOnly))
		return QString();
	const qint64 size = f.size();
	f.seek(qMax<qint64>(0, size - 4096));
	QStringList lines = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
	if (lines.size() > n)
		lines = lines.mid(lines.size() - n);
	return lines.join('\n');
}

void ServerManager::start()
{
	m_steps.clear();
	if (probe(m_opt.socketPath))
	{
		m_started = false;
		m_done = true;
		QTimer::singleShot(0, this, &ServerManager::ready);
		return;
	}

	// sockaddr_un.sun_path is about 108 bytes
	if (m_opt.socketPath.toLocal8Bit().size() > 100)
	{
		emit failed(tr("The socket path is too long for a unix socket (%1 bytes, at most 100): %2\nUse --socket with a shorter path.").arg(m_opt.socketPath.toLocal8Bit().size()).arg(m_opt.socketPath));
		return;
	}

	const QString bin = findServerBinary(m_opt.serverBinary);
	if (bin.isEmpty())
	{
		emit failed(tr("Cannot find the FatRat server (fatrat-nogui or fatrat). Pass --server-bin, or connect to a running one with --url."));
		return;
	}
	m_steps << tr("binary    %1").arg(bin);
	m_steps << tr("socket    %1").arg(m_opt.socketPath);

	QDir().mkpath(QFileInfo(m_opt.socketPath).absolutePath());
	chmod(QFileInfo(m_opt.socketPath).absolutePath().toLocal8Bit().constData(), 0700);
	QDir().mkpath(QFileInfo(m_opt.logPath).absolutePath());

	QStringList args = { "--nogui", "--rest-socket", m_opt.socketPath };
	if (!m_opt.configFile.isEmpty())
		args << "--config" << m_opt.configFile;

	QProcess p;
	p.setProgram(bin);
	p.setArguments(args);
	p.setStandardInputFile(QProcess::nullDevice());
	p.setStandardOutputFile(m_opt.logPath, QIODevice::Append);
	p.setStandardErrorFile(m_opt.logPath, QIODevice::Append);
	p.setWorkingDirectory(QDir::homePath());
	qint64 pid = 0;
	if (!p.startDetached(&pid))
	{
		emit failed(tr("Cannot start %1: %2").arg(bin, p.errorString()));
		return;
	}
	m_pid = pid;
	m_started = true;
	QFile pf(pidFile());
	if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate))
		pf.write(QByteArray::number(m_pid) + "\n");

	m_deadline = QDateTime::currentMSecsSinceEpoch() + m_opt.timeoutMs;
	m_timer.start();
	emit progress();
}

bool ServerManager::childExited() const
{
	if (m_pid <= 0)
		return false;
	int status;
	if (waitpid(pid_t(m_pid), &status, WNOHANG) == pid_t(m_pid))
		return true;
	return kill(pid_t(m_pid), 0) != 0 && errno == ESRCH;
}

void ServerManager::check()
{
	if (m_done)
		return;
	if (probe(m_opt.socketPath, 50))
	{
		m_done = true;
		m_timer.stop();
		emit ready();
		return;
	}
	if (childExited())
	{
		m_done = true;
		m_timer.stop();
		m_started = false;
		QFile::remove(pidFile());
		const QString log = lastLogLines();
		emit failed(tr("The FatRat server exited right after starting.\nIf the desktop app is already running, only one FatRat instance can run per session: enable its web interface and use --url.") + (log.isEmpty() ? QString() : "\n\n" + log));
		return;
	}
	if (QDateTime::currentMSecsSinceEpoch() > m_deadline)
	{
		m_done = true;
		m_timer.stop();
		emit failed(tr("The FatRat server did not answer within %1 seconds.\n%2").arg(m_opt.timeoutMs / 1000).arg(lastLogLines()));
		return;
	}
	emit progress();
}

void ServerManager::stop()
{
	if (m_pid > 0)
		kill(pid_t(m_pid), SIGTERM);
	QFile::remove(pidFile());
	m_started = false;
}

QString ServerManager::stopRunning(const QString& socketPath, bool* ok)
{
	*ok = false;
	QFile pf(socketPath + ".pid");
	if (!pf.open(QIODevice::ReadOnly))
		return tr("No server started by fatrat-tui is known (no %1).").arg(pf.fileName());
	const pid_t pid = pid_t(pf.readAll().trimmed().toLongLong());
	if (pid <= 0 || kill(pid, SIGTERM) != 0)
	{
		QFile::remove(pf.fileName());
		return tr("The server is not running any more.");
	}
	for (int i = 0; i < 50 && QFileInfo::exists(socketPath); i++)
		usleep(100 * 1000);
	QFile::remove(pf.fileName());
	*ok = true;
	return tr("Stopped the server (pid %1).").arg(pid);
}

}
