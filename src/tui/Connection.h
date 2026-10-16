#pragma once
#include "HttpClient.h"
#include <QString>
#include <QStringList>
#include <functional>

namespace tui
{
	// The web interface settings of a FatRat on this machine, read from its settings file
	struct LocalInstance
	{
		QString file;
		bool fileFound = false;
		bool enabled = false;
		quint16 port = 2233;
		bool ssl = false;
		QString password;
	};

	// Reads remote/enable, remote/port, remote/ssl and remote/password. An empty file means the default
	// location (~/.config/Dolezel/fatrat.conf), as used by the program itself.
	LocalInstance readLocalInstance(const QString& settingsFile);

	struct PlanOptions
	{
		enum Mode { Auto, Socket, Local, Url };
		Mode mode = Auto;
		QString url;
		QString socketPath;
		QString settingsFile;
		bool noStart = false;

		// Replaceable for tests
		std::function<bool(const QString& socketPath)> socketAnswers;
		std::function<bool(const QString& host, quint16 port)> portAnswers;
	};

	// How fatrat-tui will reach FatRat, and why
	struct Plan
	{
		enum Kind
		{
			Url,              // an address given by the user
			LocalInstance,    // a running FatRat on this machine, found through its settings
			ExistingSocket,   // a server on the unix socket that is already running
			StartPrivate,     // start a server of our own on the unix socket
			Failure,
		} kind = Failure;

		Endpoint endpoint;
		QString description;            // for the title bar
		QStringList explanation;        // what was looked at, one line each
		QString error;                  // for Failure
		QString password;               // from the settings of a local instance
		bool passwordFromSettings = false;
		QString settingsFile;
	};

	Plan planConnection(const PlanOptions& o);
	bool parseMode(const QString& s, PlanOptions::Mode* out);
	QString modeName(PlanOptions::Mode m);
	bool tcpPortAnswers(const QString& host, quint16 port, int timeoutMs = 300);
}
