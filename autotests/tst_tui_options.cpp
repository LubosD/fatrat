#include <QtTest>

class TestTuiOptions : public QObject
{
	Q_OBJECT
private slots:
	void placeholder() {}
};

QTEST_GUILESS_MAIN(TestTuiOptions)
#include "tst_tui_options.moc"
