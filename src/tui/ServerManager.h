#pragma once
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace tui
{
	// Finds, starts and stops the background FatRat server that serves the API on a unix socket
	class ServerManager : public QObject
	{
		Q_OBJECT
	public:
		struct Options
		{
			QString socketPath;     // default: defaultSocketPath()
			QString serverBinary;   // default: found next to this program or in PATH
			QString configFile;     // passed as --config
			QString logPath;        // default: defaultLogPath()
			int timeoutMs = 15000;
		};

		explicit ServerManager(const Options& o, QObject* parent = nullptr);

		static QString defaultSocketPath();
		static QString defaultLogPath();
		static QString findServerBinary(const QString& hint);
		// True if something accepts connections on the socket
		static bool probe(const QString& socketPath, int timeoutMs = 200);
		// Sends SIGTERM to a server started earlier by a TUI (from its pid file); returns a message
		static QString stopRunning(const QString& socketPath, bool* ok);

		const Options& options() const { return m_opt; }
		// Attaches to a running server or starts one; emits ready() or failed()
		void start();
		bool startedByUs() const { return m_started; }
		void stop();
		QStringList steps() const { return m_steps; }
		QString lastLogLines(int n = 6) const;

	signals:
		void progress();
		void ready();
		void failed(const QString& message);

	private slots:
		void check();

	private:
		bool childExited() const;
		QString pidFile() const { return m_opt.socketPath + ".pid"; }

		Options m_opt;
		QTimer m_timer;
		qint64 m_pid = 0;
		qint64 m_deadline = 0;
		bool m_started = false, m_done = false;
		QStringList m_steps;
	};
}
