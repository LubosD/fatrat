#include <QtTest>
#include "XmlRpc.h"

class TestXmlRpc : public QObject
{
	Q_OBJECT
private slots:
	void scalarsRoundTrip_data();
	void scalarsRoundTrip();
	void structRoundTrip();
	void stringArrayBecomesStringList();
	void mixedArrayStaysVariantList();
};

static QList<QVariant> roundTrip(const QVariant& value)
{
	QByteArray function;
	QList<QVariant> args;
	XmlRpc::parseCall(XmlRpc::createCall("f", QList<QVariant>() << value), function, args);
	return args;
}

void TestXmlRpc::scalarsRoundTrip_data()
{
	QTest::addColumn<QVariant>("value");
	QTest::addColumn<int>("type");

	QTest::newRow("int") << QVariant(42) << int(QMetaType::Int);
	QTest::newRow("bool") << QVariant(true) << int(QMetaType::Bool);
	QTest::newRow("double") << QVariant(1.5) << int(QMetaType::Double);
	QTest::newRow("string") << QVariant(QString::fromUtf8("h\xc3\xa9llo")) << int(QMetaType::QString);
}

void TestXmlRpc::scalarsRoundTrip()
{
	QFETCH(QVariant, value);
	QFETCH(int, type);

	QList<QVariant> args = roundTrip(value);
	QCOMPARE(args.size(), 1);
	QCOMPARE(args[0].userType(), type);
	QCOMPARE(args[0], value);
}

void TestXmlRpc::structRoundTrip()
{
	QVariantMap map;
	map["name"] = "x";
	map["n"] = 5;
	map["f"] = true;

	QList<QVariant> args = roundTrip(map);
	QCOMPARE(args.size(), 1);
	QCOMPARE(args[0].userType(), int(QMetaType::QVariantMap));
	QCOMPARE(args[0].toMap(), map);
}

void TestXmlRpc::stringArrayBecomesStringList()
{
	QList<QVariant> args = roundTrip(QStringList() << "a" << "b");
	QCOMPARE(args[0].userType(), int(QMetaType::QStringList));
	QCOMPARE(args[0].toStringList(), QStringList() << "a" << "b");
}

void TestXmlRpc::mixedArrayStaysVariantList()
{
	QList<QVariant> args = roundTrip(QVariantList() << "a" << 3);
	QCOMPARE(args[0].userType(), int(QMetaType::QVariantList));
	QCOMPARE(args[0].toList().size(), 2);
}

QTEST_APPLESS_MAIN(TestXmlRpc)
#include "tst_xmlrpc.moc"
