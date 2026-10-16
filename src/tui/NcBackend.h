#pragma once
#include "Canvas.h"
#include "Event.h"
#include <cwchar>
#include <vector>

namespace tui
{
	// The only place that talks to ncurses
	class NcBackend : public Canvas
	{
	public:
		NcBackend() = default;
		~NcBackend() override;

		bool init(bool mouse);
		void shutdown();

		int width() const override;
		int height() const override;
		void put(int x, int y, const QString& text, Style style) override;
		void setCursor(int x, int y, bool visible) override;

		void begin();     // starts a frame
		void flush();     // shows it
		// Reads all pending input without blocking
		void readEvents(std::vector<Event>& out);

	private:
		bool m_active = false, m_mouse = false, m_cursorVisible = false;
		int m_cx = 0, m_cy = 0;
		bool m_buttonDown = false;
		std::vector<wint_t> m_leftCodes;
		bool m_inPaste = false;
		QString m_pasteText;
		qint64 m_lastPressMs = 0;
		int m_lastPressX = -1, m_lastPressY = -1;
	};
}
