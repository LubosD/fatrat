#include <QtTest>
#include "remote/RestApi.h"
#include "RuntimeException.h"

class TestRestApi : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void pathParametersAreCaptured();
	void unknownPathIs404();
	void wrongMethodIs405WithAllow();
	void malformedJsonIs400();
	void nonJsonBodyIs415();
	void multipartOnlyOnFlaggedRoutes();
	void bodylessPostNeedsNoContentType();
	void crossOriginIs403();
	void sameOriginIsAccepted();
	void noContentHasNoBody();
	void createdSetsLocation();
	void runtimeExceptionIs500();
	void queryValuesRepeat();
	void validatorsRejectWrongTypes();
	void speedPair();
	void wholeNumbersBecomeInts();
};

static RestRequest request(const QString& method, const QString& path, const QByteArray& body = QByteArray(),
	const QString& contentType = QString())
{
	RestRequest r;
	r.method = method;
	r.path = path;
	r.body = body;
	r.contentType = contentType;
	r.host = "localhost:9091";
	return r;
}

template<typename F> static bool throwsRest(F fn)
{
	try { fn(); } catch (const RestError&) { return true; }
	return false;
}

static QJsonObject errorOf(const RestResponse& r)
{
	return QJsonDocument::fromJson(r.body).object()["error"].toObject();
}

void TestRestApi::init()
{
	RestApi::clearRoutes();
	RestApi::registerRoute("GET", "/queues/:queue/transfers", [](const RestRequest& r) {
		QJsonObject o;
		o["queue"] = r.pathParams["queue"];
		return RestResult::ok(o);
	});
	RestApi::registerRoute("POST", "/queues/:queue/transfers", [](const RestRequest& r) {
		return RestResult::created(r.jsonObject(), "/api/v1/transfers/x");
	}, true);
	RestApi::registerRoute("DELETE", "/queues/:queue/transfers", [](const RestRequest&) {
		return RestResult::noContent();
	});
	RestApi::registerRoute("POST", "/apply", [](const RestRequest&) {
		return RestResult::accepted();
	});
	RestApi::registerRoute("GET", "/boom", [](const RestRequest&) -> RestResult {
		throw RuntimeException("it broke");
	});
	RestApi::registerRoute("GET", "/q", [](const RestRequest& r) {
		QJsonArray a;
		for (const QString& s : r.queryValues("uuid"))
			a << s;
		return RestResult::ok(a);
	});
}

void TestRestApi::pathParametersAreCaptured()
{
	RestRequest r = request("GET", "/api/v1/queues/abc-123/transfers");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 200);
	QCOMPARE(res.contentType, QString("application/json"));
	QCOMPARE(QJsonDocument::fromJson(res.body).object()["queue"].toString(), QString("abc-123"));
}

void TestRestApi::unknownPathIs404()
{
	RestRequest r = request("GET", "/api/v1/nothing/here");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 404);
	QCOMPARE(errorOf(res)["code"].toString(), QString("unknown_route"));
	QVERIFY(!errorOf(res)["message"].toString().isEmpty());
}

void TestRestApi::wrongMethodIs405WithAllow()
{
	RestRequest r = request("PUT", "/api/v1/queues/q1/transfers");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 405);
	QCOMPARE(errorOf(res)["code"].toString(), QString("method_not_allowed"));
	QCOMPARE(res.headers["Allow"], QString("GET, POST, DELETE"));
}

void TestRestApi::malformedJsonIs400()
{
	RestRequest r = request("POST", "/api/v1/queues/q1/transfers", "{not json", "application/json");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 400);
	QCOMPARE(errorOf(res)["code"].toString(), QString("invalid_json"));
}

void TestRestApi::nonJsonBodyIs415()
{
	for (const QString& type : { QString("text/xml"), QString("application/x-www-form-urlencoded"), QString("text/plain"), QString() })
	{
		RestRequest r = request("POST", "/api/v1/queues/q1/transfers", "{}", type);
		RestResponse res = RestApi::dispatch(r);
		QCOMPARE(res.status, 415);
		QCOMPARE(errorOf(res)["code"].toString(), QString("unsupported_media_type"));
	}

	RestRequest ok = request("POST", "/api/v1/queues/q1/transfers", "{}", "application/json; charset=utf-8");
	QCOMPARE(RestApi::dispatch(ok).status, 201);
}

void TestRestApi::multipartOnlyOnFlaggedRoutes()
{
	RestRequest allowed = request("POST", "/api/v1/queues/q1/transfers", "x", "multipart/form-data; boundary=zz");
	allowed.formFields["options"] = "{}"; // the handler does not look at the body
	// the handler parses r.jsonObject(), so a non-JSON body gets as far as invalid_json: the guard let it through
	QCOMPARE(errorOf(RestApi::dispatch(allowed))["code"].toString(), QString("invalid_json"));

	RestRequest denied = request("DELETE", "/api/v1/queues/q1/transfers", "x", "multipart/form-data; boundary=zz");
	QCOMPARE(RestApi::dispatch(denied).status, 415);
}

void TestRestApi::bodylessPostNeedsNoContentType()
{
	RestRequest r = request("POST", "/api/v1/apply");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 202);
	QVERIFY(res.body.isEmpty());
}

void TestRestApi::crossOriginIs403()
{
	for (const QString& origin : { QString("https://evil.example"), QString("http://localhost:1234"), QString("null") })
	{
		RestRequest r = request("POST", "/api/v1/apply");
		r.origin = origin;
		RestResponse res = RestApi::dispatch(r);
		QCOMPARE(res.status, 403);
		QCOMPARE(errorOf(res)["code"].toString(), QString("forbidden_origin"));
	}

	RestRequest get = request("GET", "/api/v1/queues/q/transfers");
	get.origin = "https://evil.example";
	QCOMPARE(RestApi::dispatch(get).status, 403);
}

void TestRestApi::sameOriginIsAccepted()
{
	RestRequest r = request("POST", "/api/v1/apply");
	r.origin = "http://LOCALHOST:9091";
	QCOMPARE(RestApi::dispatch(r).status, 202);
}

void TestRestApi::noContentHasNoBody()
{
	RestRequest r = request("DELETE", "/api/v1/queues/q/transfers");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 204);
	QVERIFY(res.body.isEmpty());
	QVERIFY(res.contentType.isEmpty());
}

void TestRestApi::createdSetsLocation()
{
	RestRequest r = request("POST", "/api/v1/queues/q/transfers", "{\"a\":1}", "application/json");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 201);
	QCOMPARE(res.headers["Location"], QString("/api/v1/transfers/x"));
	QCOMPARE(QJsonDocument::fromJson(res.body).object()["a"].toInt(), 1);
}

void TestRestApi::runtimeExceptionIs500()
{
	RestRequest r = request("GET", "/api/v1/boom");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(res.status, 500);
	QCOMPARE(errorOf(res)["message"].toString(), QString("it broke"));
}

void TestRestApi::queryValuesRepeat()
{
	RestRequest r = request("GET", "/api/v1/q");
	r.query.insert("uuid", "a");
	r.query.insert("uuid", "b");
	RestResponse res = RestApi::dispatch(r);
	QCOMPARE(QJsonDocument::fromJson(res.body).array().size(), 2);
}

void TestRestApi::validatorsRejectWrongTypes()
{
	auto code = [](auto fn) {
		try { fn(); } catch (const RestError& e) { return e.status * 1000 + (e.code == "invalid_property"); }
		return 0;
	};

	QCOMPARE(code([]{ RestApi::asString(QJsonValue(1), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asBool(QJsonValue("true"), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asInt(QJsonValue(1.5), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asInt(QJsonValue(1e12), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asInt(QJsonValue("1"), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asStringList(QJsonArray() << "a" << 2, "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asStringList(QJsonValue("a"), "f"); }), 422001);
	QCOMPARE(code([]{ RestApi::asInt(QJsonValue(7.0), "f"); }), 0);
}

void TestRestApi::speedPair()
{
	int down = 0, up = 0;
	QJsonObject o;
	o["down"] = 10;
	o["up"] = 20;
	RestApi::asSpeedPair(o, "speedLimits", down, up);
	QCOMPARE(down, 10);
	QCOMPARE(up, 20);

	QCOMPARE(RestApi::speedPairToJson(3, 4)["up"].toInt(), 4);

	QJsonObject missing;
	missing["down"] = 1;
	QVERIFY(throwsRest([&]{ RestApi::asSpeedPair(missing, "speedLimits", down, up); }));
	QVERIFY(throwsRest([&]{ RestApi::asSpeedPair(QJsonArray() << 1 << 2, "speedLimits", down, up); }));

	QJsonObject extra = o;
	extra["sideways"] = 1;
	QVERIFY(throwsRest([&]{ RestApi::asSpeedPair(extra, "speedLimits", down, up); }));
}

void TestRestApi::wholeNumbersBecomeInts()
{
	QCOMPARE(RestApi::jsonToVariant(QJsonValue(9091.0)).typeId(), int(QMetaType::Int));
	QCOMPARE(RestApi::jsonToVariant(QJsonValue(9091.0)).toUInt(), 9091u);
	QCOMPARE(RestApi::jsonToVariant(QJsonValue(5e9)).typeId(), int(QMetaType::LongLong));
	QCOMPARE(RestApi::jsonToVariant(QJsonValue(1.5)).typeId(), int(QMetaType::Double));
	QCOMPARE(RestApi::jsonToVariant(QJsonValue(true)).typeId(), int(QMetaType::Bool));
	QCOMPARE(RestApi::jsonToVariant(QJsonValue("x")).toString(), QString("x"));
	QVERIFY(RestApi::variantToJson(QVariant()).isNull());
}

QTEST_APPLESS_MAIN(TestRestApi)
#include "tst_restapi.moc"
