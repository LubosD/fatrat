#include <QtTest>
class T : public QObject { Q_OBJECT private slots: void placeholder() {} };
QTEST_APPLESS_MAIN(T)
#include "tst_tui_render.moc"
