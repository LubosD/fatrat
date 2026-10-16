#include "Connection.h"
#include <QTemporaryDir>
#include <QtTest>

using namespace tui;

class TestTuiConnection : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	QString settings(const QString& body)
	{
		const QString path = m_dir.path() + "/fatrat.conf";
		QFile f(path);
		f.open(QIODevice::WriteOnly | QIODevice::Truncate);
		f.write(body.toUtf8());
		return path;
	}

	static PlanOptions options(const QString& conf, bool socketUp, int portUp)
	{
		PlanOptions o;
		o.socketPath = "/run/user/1/fatrat/rest.sock";
		o.settingsFile = conf;
		o.socketAnswers = [socketUp](const QString&) { return socketUp; };
		o.portAnswers = [portUp](const QString&, quint16 port) { return int(port) == portUp; };
		return o;
	}

private slots:
	void readsTheWebInterfaceSettings()
	{
		const LocalInstance li = readLocalInstance(settings("[remote]\nenable=true\nport=5010\nssl=true\npassword=s3cret\n"));
		QVERIFY(li.fileFound);
		QVERIFY(li.enabled);
		QCOMPARE(li.port, quint16(5010));
		QVERIFY(li.ssl);
		QCOMPARE(li.password, QString("s3cret"));
	}

	void missingSettingsMeanDisabled()
	{
		const LocalInstance li = readLocalInstance(m_dir.path() + "/nope.conf");
		QVERIFY(!li.fileFound);
		QVERIFY(!li.enabled);
	}

	void autoPrefersAnExistingSocket()
	{
		const Plan p = planConnection(options(settings("[remote]\nenable=true\nport=5010\n"), true, 5010));
		QCOMPARE(p.kind, Plan::ExistingSocket);
		QCOMPARE(p.endpoint.kind, Endpoint::Unix);
		QVERIFY(!p.explanation.isEmpty());
	}

	void autoFindsARunningFatRatThroughItsSettings()
	{
		const Plan p = planConnection(options(settings("[remote]\nenable=true\nport=5010\npassword=pw\n"), false, 5010));
		QCOMPARE(p.kind, Plan::LocalInstance);
		QCOMPARE(p.endpoint.kind, Endpoint::Tcp);
		QCOMPARE(p.endpoint.host, QString("127.0.0.1"));
		QCOMPARE(p.endpoint.port, quint16(5010));
		QCOMPARE(p.password, QString("pw"));
		QVERIFY(p.passwordFromSettings);
		QVERIFY(p.description.contains("5010"));
		// the explanation says that the socket was looked at first
		QVERIFY(p.explanation.join('\n').contains("nothing is listening"));
	}

	void httpsFollowsTheSslSetting()
	{
		const Plan p = planConnection(options(settings("[remote]\nenable=true\nport=5010\nssl=true\n"), false, 5010));
		QCOMPARE(p.kind, Plan::LocalInstance);
		QCOMPARE(p.endpoint.kind, Endpoint::Tls);
		QVERIFY(!p.passwordFromSettings);
	}

	void enabledButNotRunningStartsAPrivateServer()
	{
		const Plan p = planConnection(options(settings("[remote]\nenable=true\nport=5010\n"), false, 0));
		QCOMPARE(p.kind, Plan::StartPrivate);
		QVERIFY(p.explanation.join('\n').contains("not running"));
	}

	void disabledWebInterfaceStartsAPrivateServer()
	{
		const Plan p = planConnection(options(settings("[remote]\nenable=false\nport=5010\n"), false, 5010));
		QCOMPARE(p.kind, Plan::StartPrivate);
		QCOMPARE(p.endpoint.socketPath, QString("/run/user/1/fatrat/rest.sock"));
		QVERIFY(p.explanation.join('\n').contains("turned off"));
	}

	void noStartFailsWithAnExplanation()
	{
		PlanOptions o = options(settings("[remote]\nenable=false\n"), false, 0);
		o.noStart = true;
		const Plan p = planConnection(o);
		QCOMPARE(p.kind, Plan::Failure);
		QVERIFY(p.error.contains("--no-start"));
	}

	void localModeNeverStartsAnything()
	{
		PlanOptions o = options(settings("[remote]\nenable=false\n"), true, 0);
		o.mode = PlanOptions::Local;
		const Plan p = planConnection(o);
		QCOMPARE(p.kind, Plan::Failure);               // even though a socket server exists
		QVERIFY(p.error.contains("web interface"));

		o = options(settings("[remote]\nenable=true\nport=7\n"), false, 7);
		o.mode = PlanOptions::Local;
		QCOMPARE(planConnection(o).kind, Plan::LocalInstance);
	}

	void socketModeIgnoresTheWebInterface()
	{
		PlanOptions o = options(settings("[remote]\nenable=true\nport=5010\n"), false, 5010);
		o.mode = PlanOptions::Socket;
		QCOMPARE(planConnection(o).kind, Plan::StartPrivate);
		o.socketAnswers = [](const QString&) { return true; };
		QCOMPARE(planConnection(o).kind, Plan::ExistingSocket);
		o.socketAnswers = [](const QString&) { return false; };
		o.noStart = true;
		QCOMPARE(planConnection(o).kind, Plan::Failure);
	}

	void anUrlWinsAndHidesItsPassword()
	{
		PlanOptions o = options(settings("[remote]\nenable=true\nport=5010\n"), true, 5010);
		o.url = "https://user:secret@example.org:8443/";
		const Plan p = planConnection(o);
		QCOMPARE(p.kind, Plan::Url);
		QCOMPARE(p.endpoint.kind, Endpoint::Tls);
		QCOMPARE(p.endpoint.port, quint16(8443));
		QVERIFY(!p.description.contains("secret"));
		QVERIFY(!p.description.contains("user"));

		o.url = "ftp://nope";
		QCOMPARE(planConnection(o).kind, Plan::Failure);
		o.url.clear();
		o.mode = PlanOptions::Url;
		QCOMPARE(planConnection(o).kind, Plan::Failure);
	}

	void modeNames()
	{
		PlanOptions::Mode m;
		QVERIFY(parseMode("AUTO", &m) && m == PlanOptions::Auto);
		QVERIFY(parseMode("private", &m) && m == PlanOptions::Socket);
		QVERIFY(parseMode("local", &m) && m == PlanOptions::Local);
		QVERIFY(!parseMode("bogus", &m));
	}
};

QTEST_MAIN(TestTuiConnection)
#include "tst_tui_connection.moc"
