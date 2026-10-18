#pragma once
#include "HttpClient.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace tui
{
	struct ApiResult
	{
		int status = 0;            // 0 = transport failure
		QJsonValue json;           // undefined if the body was empty or not JSON
		QByteArray raw;
		QString errorCode;         // from {"error":{"code","message"}}
		QString errorMessage;      // server message, or the transport error
		bool ok() const { return status >= 200 && status < 300; }
		bool transportFailure() const { return status == 0; }
		bool unauthorized() const { return status == 401; }
		QString describe() const;  // one line for the status bar
	};

	struct AddRequest
	{
		QStringList urls;
		bool upload = false;
		QString cls;               // empty = autodetect
		QString target;
		bool paused = false;
		qint64 downLimit = 0, upLimit = 0;
		QJsonObject toJson(bool withUrls = true) const;
	};

	struct DeleteQueueOptions
	{
		QString moveTo;
		bool deleteTransfers = false, withData = false;
	};

	// Typed wrapper around the REST API. Every call is asynchronous; the callback runs on the Qt event loop.
	class ApiClient : public QObject
	{
		Q_OBJECT
	public:
		using Callback = std::function<void(const ApiResult&)>;

		explicit ApiClient(HttpClient* http, QObject* parent = nullptr);
		HttpClient* http() const { return m_http; }

		static QString encode(const QString& s);

		void call(const QByteArray& method, const QString& path, const QJsonValue& body, Callback cb);
		void get(const QString& path, Callback cb) { call("GET", path, QJsonValue(), std::move(cb)); }

		void stats(Callback cb);
		void queues(Callback cb);
		void queueTransfers(const QString& queue, Callback cb);
		void transferClasses(Callback cb);
		void transferDetails(const QString& transfer, Callback cb);
		void transferOptions(const QString& transfer, Callback cb);                          // json: {fields, values}
		void patchTransferOptions(const QString& transfer, const QJsonObject& values, Callback cb);
		void listDirectories(const QString& path, const QString& queue, bool showHidden, Callback cb);

		void createQueue(const QJsonObject& props, Callback cb);
		void patchQueue(const QString& queue, const QJsonObject& props, Callback cb);
		void deleteQueue(const QString& queue, const DeleteQueueOptions& o, Callback cb);
		void moveQueue(const QString& queue, const QString& direction, Callback cb);

		void addTransfers(const QString& queue, const AddRequest& r, Callback cb);
		void uploadTransfer(const QString& queue, const QString& filePath, const AddRequest& r, Callback cb);
		void patchTransfers(const QStringList& uuids, const QJsonObject& props, Callback cb);
		void patchTransfer(const QString& uuid, const QJsonObject& props, Callback cb);
		void deleteTransfers(const QStringList& uuids, bool withData, Callback cb);
		void moveTransfers(const QString& queue, const QStringList& uuids, const QString& direction, Callback cb);
		void moveTransfersBefore(const QString& queue, const QStringList& uuids, const QString& before, Callback cb);
		void setFilePriorities(const QString& transfer, const QJsonArray& items, Callback cb);

		void globalLog(Callback cb);
		void transferLog(const QString& transfer, Callback cb);

		// Settings
		void settingsPages(Callback cb);
		void schedulerActions(Callback cb);
		void createSchedulerAction(const QJsonObject& props, Callback cb);
		void patchSchedulerAction(const QString& id, const QJsonObject& props, Callback cb);
		void deleteSchedulerAction(const QString& id, Callback cb);
		void settingsValues(const QStringList& keys, Callback cb);       // json: object key -> value
		void patchSettings(const QJsonObject& values, Callback cb);
		void applySettings(Callback cb);
		void generateCertificate(const QString& hostname, Callback cb);  // json: {"path"}

		// RSS
		void rssFeeds(Callback cb);
		void rssRegexps(Callback cb);
		void rssTest(const QJsonObject& body, Callback cb);

	private:
		HttpClient* m_http;
	};
}
