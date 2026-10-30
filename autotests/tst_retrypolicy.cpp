#include <QTest>
#include "RetryPolicy.h"

class tst_retrypolicy : public QObject
{
	Q_OBJECT
private slots:
	void workingTransferRetriesWithDefaults()
	{
		// defaults: retrycount=0, retryworking=true
		QVERIFY(RetryPolicy::shouldRetry(true, true, 0, 0));
		QVERIFY(RetryPolicy::shouldRetry(true, true, 0, 50));
	}
	void neverWorkingTransferDoesNotRetryWithDefaults()
	{
		QVERIFY(!RetryPolicy::shouldRetry(true, false, 0, 0));
	}
	void retryCountIsHonoured()
	{
		QVERIFY(RetryPolicy::shouldRetry(false, false, 3, 2));
		QVERIFY(!RetryPolicy::shouldRetry(false, false, 3, 3));
		QVERIFY(RetryPolicy::shouldRetry(true, false, 3, 2));
		QVERIFY(!RetryPolicy::shouldRetry(true, false, 3, 3));
	}
	void workingFlagIgnoredWhenOptionOff()
	{
		QVERIFY(!RetryPolicy::shouldRetry(false, true, 0, 0));
	}
};

QTEST_APPLESS_MAIN(tst_retrypolicy)
#include "tst_retrypolicy.moc"
