#include "core/display.h"

#include <QTest>

using namespace contribusage;

// Sunday 2026-10-04, 09:41 local time, as in the macOS screenshot.
static const QDateTime now(QDate(2026, 10, 4), QTime(9, 41));

class TestDisplay : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void meterLevelFollowsTheThresholds() {
        QCOMPARE(meterLevel(69.9), MeterLevel::Normal);
        QCOMPARE(meterLevel(70), MeterLevel::Warning);
        QCOMPARE(meterLevel(89.9), MeterLevel::Warning);
        QCOMPARE(meterLevel(90), MeterLevel::Critical);
        QCOMPARE(meterLevel(140), MeterLevel::Critical);
    }

    void barFractionIsClamped() {
        QCOMPARE(barFraction(23), 0.23);
        QCOMPARE(barFraction(140), 1.0);
        QCOMPARE(barFraction(-5), 0.0);
    }

    void aResetWindowDrawsEmptyAndNormal() {
        // FR-11: past its reset the value is unknown, so the bar is empty and not red.
        QCOMPARE(barFraction(97, now.addSecs(-60), now), 0.0);
        QCOMPARE(meterLevel(97, now.addSecs(-60), now), MeterLevel::Normal);
        QCOMPARE(barFraction(97, now.addSecs(60), now), 0.97);
        QCOMPARE(meterLevel(97, now.addSecs(60), now), MeterLevel::Critical);
        QCOMPARE(barFraction(97, std::nullopt, now), 0.97);
    }

    void shownWindowLabel_data() {
        QTest::addColumn<QString>("printed");
        QTest::addColumn<QString>("shown");
        QTest::newRow("session") << QStringLiteral("Current session") << QStringLiteral("Current session");
        QTest::newRow("all models") << QStringLiteral("Current week (all models)") << QStringLiteral("Current week");
        QTest::newRow("one model") << QStringLiteral("Current week (Opus)") << QString();
        QTest::newRow("another model") << QStringLiteral("Current week (Sonnet)") << QString();
        QTest::newRow("unknown window") << QStringLiteral("Daily requests") << QStringLiteral("Daily requests");
    }
    void shownWindowLabel() {
        // ADR-042: one week bar; a model's own week window is hidden, an empty result means hidden.
        QFETCH(QString, printed);
        QFETCH(QString, shown);
        QCOMPARE(contribusage::shownWindowLabel(printed).value_or(QString()), shown);
    }

    void barFractionsScaleToTheBusiestDay() {
        QCOMPARE(barFractions({1, 2, 4}), (QList<double>{0.25, 0.5, 1.0}));
        QCOMPARE(barFractions({0, 0, 0}), (QList<double>{0.0, 0.0, 0.0}));
        QCOMPARE(barFractions({}), QList<double>{});
    }

    void percentText_data() {
        QTest::addColumn<double>("used");
        QTest::addColumn<bool>("belowOne");
        QTest::addColumn<qint64>("resetsIn");
        QTest::addColumn<QString>("expected");
        QTest::newRow("value") << 23.7 << false << qint64(7800) << QStringLiteral("23%");
        QTest::newRow("full") << 100.0 << false << qint64(7800) << QStringLiteral("100%");
        QTest::newRow("below one") << 0.4 << true << qint64(7800) << QStringLiteral("<1%");
        QTest::newRow("reset passed") << 97.0 << false << qint64(-60) << QStringLiteral("–");
    }
    void percentText() {
        QFETCH(double, used);
        QFETCH(bool, belowOne);
        QFETCH(qint64, resetsIn);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::percentText(used, belowOne, now.addSecs(resetsIn), now), expected);
    }

    void percentTextWithoutResetTime() { QCOMPARE(contribusage::percentText(41, false, std::nullopt, now), u"41%"); }

    void resetText_data() {
        QTest::addColumn<qint64>("resetsIn");
        QTest::addColumn<QString>("expected");
        QTest::newRow("hours and minutes") << qint64(7800) << QStringLiteral("resets in 2 hr 10 min");
        QTest::newRow("whole hour") << qint64(3600) << QStringLiteral("resets in 1 hr");
        QTest::newRow("minutes") << qint64(720) << QStringLiteral("resets in 12 min");
        QTest::newRow("never below one minute") << qint64(20) << QStringLiteral("resets in 1 min");
        QTest::newRow("exactly a day") << qint64(86400) << QStringLiteral("resets in 24 hr");
        QTest::newRow("more than a day") << qint64(4 * 86400 + 4 * 3600 + 17 * 60)
                                         << QStringLiteral("resets Thu 13:58");
        QTest::newRow("passed") << qint64(0) << QStringLiteral("reset, refreshing…");
    }
    void resetText() {
        QFETCH(qint64, resetsIn);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::resetText(now.addSecs(resetsIn), now, QLocale::c()), std::optional<QString>(expected));
    }

    void resetTextWithoutResetTime() { QCOMPARE(contribusage::resetText(std::nullopt, now), std::nullopt); }

    void ageText_data() {
        QTest::addColumn<qint64>("age");
        QTest::addColumn<QString>("expected");
        QTest::newRow("just now") << qint64(59) << QStringLiteral("updated just now");
        QTest::newRow("minutes") << qint64(120) << QStringLiteral("updated 2 min ago");
        QTest::newRow("hours") << qint64(3 * 3600 + 59 * 60) << QStringLiteral("updated 3 hr ago");
        QTest::newRow("one day") << qint64(86400) << QStringLiteral("updated 1 day ago");
        QTest::newRow("days") << qint64(3 * 86400) << QStringLiteral("updated 3 days ago");
    }
    void ageText() {
        QFETCH(qint64, age);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::ageText(now.addSecs(-age), now), expected);
    }

    void compactCount_data() {
        QTest::addColumn<qint64>("value");
        QTest::addColumn<QString>("expected");
        QTest::newRow("small") << qint64(668) << QStringLiteral("668");
        QTest::newRow("thousand, one decimal") << qint64(1234) << QStringLiteral("1.2K");
        QTest::newRow("thousand, no zero decimal") << qint64(2000) << QStringLiteral("2K");
        QTest::newRow("tens of thousands") << qint64(12000) << QStringLiteral("12K");
        QTest::newRow("hundreds of thousands") << qint64(812000) << QStringLiteral("812K");
        QTest::newRow("millions") << qint64(5324000) << QStringLiteral("5.3M");
        QTest::newRow("tens of millions") << qint64(41000000) << QStringLiteral("41M");
    }
    void compactCount() {
        QFETCH(qint64, value);
        QFETCH(QString, expected);
        QCOMPARE(contribusage::compactCount(value), expected);
    }

    void meterLevelNames() {
        QCOMPARE(meterLevelName(MeterLevel::Normal), QStringLiteral("normal"));
        QCOMPARE(meterLevelName(MeterLevel::Warning), QStringLiteral("warning"));
        QCOMPARE(meterLevelName(MeterLevel::Critical), QStringLiteral("critical"));
    }

    // SPEC §13, worded once with the source's name.
    void errorMessages() {
        const QString name = QStringLiteral("Claude Code");
        QCOMPARE(errorMessage({.kind = SourceError::Kind::ToolNotFound}, name),
                 QStringLiteral("Claude Code wasn't found"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::NotLoggedIn}, name),
                 QStringLiteral("Claude Code isn't logged in"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::TimedOut}, name),
                 QStringLiteral("Claude Code didn't answer in time"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::ProcessFailed, .code = 2}, name),
                 QStringLiteral("Exited with code 2"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::Unparseable}, name),
                 QStringLiteral("Couldn't read the usage output"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::Io, .text = QStringLiteral("disk full")}, name),
                 QStringLiteral("disk full"));
        QCOMPARE(errorMessage({.kind = SourceError::Kind::UnsupportedPlan, .text = QStringLiteral("note")}, name),
                 QStringLiteral("note"));
    }
};

QTEST_GUILESS_MAIN(TestDisplay)
#include "tst_display.moc"
