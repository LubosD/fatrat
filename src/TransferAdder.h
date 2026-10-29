/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2026 Lubos Dolezel <lubos a dolezel.info>

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

#ifndef TRANSFERADDER_H
#define TRANSFERADDER_H
#include <QStringList>
#include "RuntimeException.h"

class Queue;

// Creates transfers from URLs or local files and appends them to a queue. Used by the REST API and watch folders.
namespace TransferAdder
{
	struct Options
	{
		QString engine;   // transfer class; empty = detected for each source
		QString target;   // empty for a download = the queue's directory
		bool upload = false;
		bool paused = false;
		int down = 0, up = 0;
	};

	// code() is "no_matching_engine", "create_failed" or "transfer_failed"
	class Error : public RuntimeException
	{
	public:
		Error(const QString& code, const QString& message) : RuntimeException(message), m_code(code) {}
		QString code() const { return m_code; }
	private:
		QString m_code;
	};

	// Returns the UUIDs of the new transfers. A source no class accepts throws Error, unless rejected is given:
	// then it is listed there and skipped. When Error is thrown nothing has been added to q.
	// The caller must not hold q's own lock.
	QStringList add(Queue* q, const QStringList& sources, Options opts, QStringList* rejected = nullptr);
}

#endif
