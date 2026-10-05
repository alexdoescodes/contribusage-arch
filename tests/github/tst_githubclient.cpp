#include "github/githubclient.h"
#include "support/fakehttptransport.h"

#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <QTimeZone>
#include <QTimer>

using namespace contribusage;
using namespace contribusage::github;

namespace {

QByteArray fixture(const char *name) {
    QFile file(QStringLiteral(CONTRIBUSAGE_FIXTURES "/github/") + QString::fromLatin1(name) + QStringLiteral(".json"));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

template <typename T> T wait(const std::function<void(std::function<void(T)>)> &start) {
    std::optional<T> result;
    QEventLoop loop;
    start([&](T value) {
        result = std::move(value);
        loop.quit();
    });
    if (!result) {
        QTimer::singleShot(5'000, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return std::move(*result);
}

QByteArray header(const HttpRequest &request, const char *name) {
    for (const auto &[key, value] : request.headers) {
        if (key.compare(name, Qt::CaseInsensitive) == 0) {
            return value;
        }
    }
    return {};
}

QJsonObject sentBody(const HttpRequest &request) {
    return QJsonDocument::fromJson(request.body).object();
}

using CalendarOutcome = std::expected<ContributionCalendar, SourceError>;

CalendarOutcome contributions(FakeHttpTransport &transport, const QDateTime &from, const QDateTime &to) {
    GitHubClient client(transport, QStringLiteral("1.0"));
    return wait<CalendarOutcome>(
        [&](auto done) { client.contributions(QStringLiteral("ghp_x"), from, to, nullptr, done); });
}

} // namespace

// SPEC §8.4, FR-17, FR-18, ADR-020; ported from GitHubClientTests.swift.
class TestGitHubClient : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // NFR-13: the widget talks to api.github.com over HTTPS and nowhere else.
    void endpointIsGitHubOverHttps() {
        QCOMPARE(graphqlEndpoint().scheme(), QStringLiteral("https"));
        QCOMPARE(graphqlEndpoint().host(), QStringLiteral("api.github.com"));
    }

    void viewerLoginSendsTokenAndReturnsLogin() {
        FakeHttpTransport transport;
        transport.respond(200, R"({"data":{"viewer":{"login":"octocat"}}})");
        GitHubClient client(transport, QStringLiteral("1.0"));
        const auto login = wait<std::expected<QString, SourceError>>(
            [&](auto done) { client.viewerLogin(QStringLiteral("ghp_x"), nullptr, done); });
        QVERIFY(login);
        QCOMPARE(*login, QStringLiteral("octocat"));
        const HttpRequest &request = transport.requests.first();
        QCOMPARE(request.url, graphqlEndpoint());
        QCOMPARE(header(request, "Authorization"), QByteArray("Bearer ghp_x"));
        QCOMPARE(header(request, "User-Agent"), QByteArray("contribusage/1.0"));
        QCOMPARE(header(request, "Content-Type"), QByteArray("application/json"));
        QVERIFY(
            sentBody(request).value(QStringLiteral("query")).toString().contains(QStringLiteral("viewer { login }")));
    }

    void contributionsDecodesTheCalendar() {
        FakeHttpTransport transport;
        transport.respond(200, fixture("calendar"));
        const QTimeZone tokyo("Asia/Tokyo");
        const QDateTime from(QDate(2025, 9, 30), QTime(0, 0), tokyo);
        const QDateTime to(QDate(2026, 9, 29), QTime(15, 0), tokyo);
        const auto calendar = contributions(transport, from, to);
        QVERIFY(calendar);
        QCOMPARE(calendar->login, QStringLiteral("octocat"));
        QCOMPARE(calendar->totalContributions, 1917);
        QList<qsizetype> sizes;
        for (const auto &week : calendar->weeks) {
            sizes.append(week.size());
        }
        QList<qsizetype> expected{5};
        expected.append(QList<qsizetype>(51, 7));
        expected.append(3);
        QCOMPARE(sizes, expected);
        const auto days = calendar->days();
        QCOMPARE(days.size(), 365);
        QVERIFY(days.first() == (ContributionDay{.date = QDate(2025, 9, 30), .count = 0, .level = 0}));
        QVERIFY(days.last() == (ContributionDay{.date = QDate(2026, 9, 29), .count = 28, .level = 4}));
        // Levels rise with the count, which pins Appendix C's name order.
        auto byCount = days;
        std::sort(byCount.begin(), byCount.end(), [](const auto &a, const auto &b) { return a.count < b.count; });
        for (qsizetype index = 1; index < byCount.size(); ++index) {
            QVERIFY(byCount.at(index - 1).level <= byCount.at(index).level);
        }
        const QJsonObject body = sentBody(transport.requests.first());
        QVERIFY(body.value(QStringLiteral("query"))
                    .toString()
                    .contains(QStringLiteral("contributionsCollection(from: $from, to: $to)")));
        // SPEC §8.4.1, Appendix C: local time with its offset.
        const QJsonObject variables = body.value(QStringLiteral("variables")).toObject();
        QCOMPARE(variables.value(QStringLiteral("from")).toString(), QStringLiteral("2025-09-30T00:00:00+09:00"));
        QCOMPARE(variables.value(QStringLiteral("to")).toString(), QStringLiteral("2026-09-29T15:00:00+09:00"));
    }

    // SPEC §8.4.3, ADR-020.
    void sendMapsFailures_data() {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("remaining");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("text");
        const auto kind = [](SourceError::Kind value) { return int(value); };
        QTest::newRow("401") << 401 << QByteArray("4958") << kind(SourceError::Kind::Unauthorized) << QString();
        QTest::newRow("200 exhausted") << 200 << QByteArray("0") << kind(SourceError::Kind::RateLimited) << QString();
        QTest::newRow("403 exhausted") << 403 << QByteArray("0") << kind(SourceError::Kind::RateLimited) << QString();
        QTest::newRow("429 exhausted") << 429 << QByteArray("0") << kind(SourceError::Kind::RateLimited) << QString();
        QTest::newRow("200 errors") << 200 << QByteArray("4958") << kind(SourceError::Kind::Decoding)
                                    << QStringLiteral("API rate limit exceeded for user ID 1.");
        QTest::newRow("403 with budget") << 403 << QByteArray("4958") << kind(SourceError::Kind::Http) << QString();
    }
    void sendMapsFailures() {
        QFETCH(int, status);
        QFETCH(QByteArray, remaining);
        QFETCH(int, kind);
        QFETCH(QString, text);
        FakeHttpTransport transport;
        transport.respond(status, fixture("graphql-errors"),
                          {{"x-ratelimit-remaining", remaining}, {"x-ratelimit-reset", "1790719818"}});
        const auto outcome = contributions(transport, QDateTime::currentDateTime(), QDateTime::currentDateTime());
        QVERIFY(!outcome);
        QCOMPARE(int(outcome.error().kind), kind);
        if (outcome.error().kind == SourceError::Kind::RateLimited) {
            QCOMPARE(outcome.error().until, QDateTime::fromSecsSinceEpoch(1790719818));
        }
        if (outcome.error().kind == SourceError::Kind::Http) {
            QCOMPARE(outcome.error().code, 403);
        }
        if (!text.isEmpty()) {
            QCOMPARE(outcome.error().text, text);
        }
    }

    // A level GitHub adds later fails loudly instead of drawing as empty.
    void contributionsRejectsAnUnknownLevel() {
        FakeHttpTransport transport;
        transport.respond(200, fixture("calendar").replace("\"NONE\"", "\"FIFTH\""));
        const auto outcome = contributions(transport, QDateTime::currentDateTime(), QDateTime::currentDateTime());
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::Decoding);
        QCOMPARE(outcome.error().text, QStringLiteral("Unknown contribution level FIFTH"));
    }

    void transportFailuresPassThrough() {
        FakeHttpTransport transport;
        transport.failure = SourceError{.kind = SourceError::Kind::Offline};
        const auto outcome = contributions(transport, QDateTime::currentDateTime(), QDateTime::currentDateTime());
        QVERIFY(!outcome);
        QCOMPARE(outcome.error().kind, SourceError::Kind::Offline);
    }
};

QTEST_GUILESS_MAIN(TestGitHubClient)
#include "tst_githubclient.moc"
