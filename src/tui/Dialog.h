#pragma once
#include "Actions.h"
#include "Canvas.h"
#include <QStringList>
#include <functional>

namespace tui
{
	// A hint such as "R" + "Resume", drawn as a highlighted key followed by the label
	int drawHint(Canvas& c, int x, int y, const QString& key, const QString& label, Style base, bool enabled = true);
	int hintWidth(const QString& key, const QString& label);

	struct Hint
	{
		QString key, label;
		std::function<void()> fn;
	};

	// Modal window. Draws itself centred and handles its own keys and mouse clicks.
	class Dialog
	{
	public:
		virtual ~Dialog() = default;
		virtual void draw(Canvas& c, HitMap& hits, const Rect& screen) = 0;
		virtual void handle(const Event& e) = 0;
		bool finished() const { return m_finished; }
		void close() { m_finished = true; }
		// Called when the pointer is pressed outside the window
		virtual void outsideClick() { close(); }
		const Rect& rect() const { return m_rect; }

	protected:
		Rect place(const Rect& screen, int w, int h);
		void drawFrame(Canvas& c, const QString& title);
		void addClick(const Rect& r, std::function<void()> fn) { m_clicks.push_back({ r, std::move(fn) }); }
		// Draws the hints on one row; every hint becomes clickable
		void drawHints(Canvas& c, int x, int y, int w, const QVector<Hint>& hints);
		// Handles mouse presses; returns true if the event was a mouse event (consumed)
		bool handleMouse(const Event& e);
		void beginDraw(HitMap& hits, const Rect& screen);

		struct Click { Rect r; std::function<void()> fn; };
		QVector<Click> m_clicks;
		Rect m_rect, m_screen;
		bool m_finished = false;
	};

	// ---------------------------------------------------------------- form

	struct FormField
	{
		enum Type { Text, Select, Check, Radio, Label, Spacer } type = Text;
		QString id, label;
		// Text
		QString text, placeholder, suffix;
		int cursor = 0, scroll = 0;
		bool secret = false;
		// Select / Radio
		QStringList options, values;
		QString hotkeys;               // Radio: one key per option
		int index = 0;
		// Check
		bool checked = false;
		std::function<void()> onChange;

		bool focusable() const { return type != Label && type != Spacer; }
	};

	class FormDialog : public Dialog
	{
	public:
		FormDialog(const QString& title, int width);

		FormField& addText(const QString& id, const QString& label, const QString& value = QString(), const QString& suffix = QString());
		FormField& addSelect(const QString& id, const QString& label, const QStringList& options, const QStringList& values, int index = 0);
		FormField& addCheck(const QString& id, const QString& label, bool checked);
		FormField& addRadio(const QString& id, const QString& label, const QStringList& options, const QString& hotkeys = QString(), int index = 0);
		void addLabel(const QString& text);
		void addSpacer();

		QString text(const QString& id) const;
		void setText(const QString& id, const QString& value);
		int index(const QString& id) const;
		QString value(const QString& id) const;      // value of the selected option
		bool checked(const QString& id) const;
		FormField* field(const QString& id);

		// Return true to close the dialog; call setError() and return false to keep it open
		void setSubmit(const QString& label, std::function<bool(FormDialog&)> fn);
		void setCancel(std::function<void()> fn) { m_onCancel = std::move(fn); }
		void setError(const QString& msg) { m_error = msg; }
		void addShortcut(int key, bool ctrl, const QString& keyLabel, const QString& label, std::function<void(FormDialog&)> fn);
		void focusField(const QString& id);
		QString focusedId() const { return m_focus >= 0 && m_focus < m_fields.size() ? m_fields[m_focus].id : QString(); }

		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
		// A stray click must not throw away what was typed
		void outsideClick() override {}

	private:
		struct Shortcut { int key; bool ctrl; QString keyLabel, label; std::function<void(FormDialog&)> fn; };

		int fieldHeight(const FormField& f, int innerWidth) const;
		bool focusedIsText() const;
		void moveFocus(int dir);
		void handleText(FormField& f, const Event& e);
		void changeSelect(FormField& f, int dir);
		void submit();
		void cancel();
		QStringList wrap(const QString& text, int width) const;

		QString m_title, m_submitLabel = "OK", m_error;
		int m_width;
		QVector<FormField> m_fields;
		int m_focus = 0;
		bool m_focusInit = false;
		std::function<bool(FormDialog&)> m_onSubmit;
		std::function<void()> m_onCancel;
		QVector<Shortcut> m_shortcuts;
	};

	// ------------------------------------------------------------- choices

	struct Choice
	{
		QString key;      // single letter
		QString label;
		int result = 0;
	};

	// A message with lettered choices. Esc, or any click outside, gives result -1.
	class ChoiceDialog : public Dialog
	{
	public:
		ChoiceDialog(const QString& title, const QStringList& message, const QVector<Choice>& choices, std::function<void(int)> done);
		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
		void outsideClick() override { if (m_cancelOnOutside) finish(-1); }
		void setCancelOnOutside(bool b) { m_cancelOnOutside = b; }
	private:
		bool m_cancelOnOutside = true;
		void finish(int result);
		QString m_title;
		QStringList m_message;
		QVector<Choice> m_choices;
		std::function<void(int)> m_done;
	};

	// Pop-up list of commands, opened by a right click or by a key
	struct MenuItem
	{
		QString label, key;
		bool enabled = true, separator = false;
		std::function<void()> fn;
		static MenuItem sep() { MenuItem i; i.separator = true; return i; }
	};

	class MenuDialog : public Dialog
	{
	public:
		MenuDialog(const QVector<MenuItem>& items, int x, int y, const QString& title = QString());
		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
	private:
		void move(int dir);
		void choose(int i);
		QVector<MenuItem> m_items;
		QString m_title;
		int m_x, m_y, m_sel = 0;
	};

	// Read-only scrollable text (help, test results)
	class TextDialog : public Dialog
	{
	public:
		TextDialog(const QString& title, const QStringList& lines, int width = 76);
		void draw(Canvas& c, HitMap& hits, const Rect& screen) override;
		void handle(const Event& e) override;
	private:
		QString m_title;
		QStringList m_lines;
		int m_width, m_top = 0, m_visible = 10;
	};
}
