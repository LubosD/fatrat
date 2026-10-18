#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace tui
{
	class FormPanel;

	// A transfer class's options (GET /transfers/<uuid>/options) as form fields, and back.
	// The vocabulary is src/plugins/FormValidator.h's; types the TUI cannot edit become a note.
	namespace options
	{
		// One field per item, with the id "opt:<key>", starting from `values`
		void addFields(FormPanel& panel, const QJsonArray& fields, const QJsonObject& values);
		// The values that differ from `values`, typed for the server. Returns a message, and
		// leaves `out` untouched, when a field does not hold a valid value.
		QString changed(const FormPanel& panel, const QJsonArray& fields, const QJsonObject& values, QJsonObject* out);
	}
}
