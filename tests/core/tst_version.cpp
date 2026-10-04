#include "core/version.h"

#include <QTest>

class TestVersion : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reportsTheProjectVersion() { QCOMPARE(contribusage::version(), QStringLiteral("0.1.0")); }
};

QTEST_GUILESS_MAIN(TestVersion)
#include "tst_version.moc"
