#pragma once
#include "App.h"
#include <QObject>
#include <QStringList>

namespace tui
{
	// Runs a list of commands against the App and a TextCanvas, for integration tests and screenshots:
	//   key <name>            Enter Esc Tab BackTab Up Down Left Right PgUp PgDn Home End Backspace Delete Space F1..F12, ctrl-<letter>, or one character
	//   type <text>           one key press per character
	//   paste <text>
	//   mouse <press|release|drag|wheelup|wheeldown> <x> <y> [button] [ctrl] [shift] [double]
	//   wait <ms>
	//   waitfor <text> [timeout ms]   until the screen contains the text
	//   expect <text>         fails the script if the screen does not contain the text
	//   reject <text>         fails if it does
	//   dump [title]          prints the screen
	// Lines starting with # are comments.
	class ScriptRunner : public QObject
	{
		Q_OBJECT
	public:
		ScriptRunner(App& app, TextCanvas& canvas, const QStringList& lines, QObject* parent = nullptr);
		void start();
		static bool parseKey(const QString& name, Event& out);

	signals:
		void finished(int exitCode);

	private:
		void next();
		bool run(const QString& line);
		void redraw();
		void fail(const QString& msg);
		QString screenText();

		App& m_app;
		TextCanvas& m_canvas;
		QStringList m_lines;
		int m_pos = 0;
		int m_rc = 0;
		// waitfor state
		QString m_waitText;
		qint64 m_waitDeadline = 0;
	};
}
