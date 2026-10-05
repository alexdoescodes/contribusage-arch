#include "github/githubaccount.h"
#include "support/fakehttptransport.h"
#include "support/fakesecretstore.h"

#include <QEventLoop>
#include <QFile>
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

using LoginOutcome = std::expected<QString, SourceError>;
using ReportOutcome = std::expected<GitHubReport, SourceError>;

} // namespace

// FR-16, FR-17, FR-18, SPEC §14; ported from GitHubAccountTests.swift.
class TestGitHubAccount : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void connectSavesAValidatedToken() {
        FakeHttpTransport transport;
        transport.respond(200, R"({"data":{"viewer":{"login":"octocat"}}})");
        FakeSecretStore secrets;
        GitHubClient client(transport, QStringLiteral("1.0"));
        GitHubAccount account(secrets, client);
        const auto login =
            wait<LoginOutcome>([&](auto done) { account.connect(QStringLiteral("ghp_x"), nullptr, done); });
        QVERIFY(login);
        QCOMPARE(*login, QStringLiteral("octocat"));
        QCOMPARE(secrets.values.value(QStringLiteral("token")), QStringLiteral("ghp_x"));
    }

    // SPEC §14: a rejected token is never saved, and a saved one survives the attempt.
    void connectKeepsARejectedTokenOut() {
        FakeHttpTransport transport;
        transport.respond(401, "{}");
        FakeSecretStore secrets;
        secrets.values.insert(QStringLiteral("token"), QStringLiteral("old"));
        GitHubClient client(transport, QStringLiteral("1.0"));
        GitHubAccount account(secrets, client);
        const auto login =
            wait<LoginOutcome>([&](auto done) { account.connect(QStringLiteral("bad"), nullptr, done); });
        QVERIFY(!login);
        QCOMPARE(login.error().kind, SourceError::Kind::Unauthorized);
        QCOMPARE(secrets.values.value(QStringLiteral("token")), QStringLiteral("old"));
    }

    void disconnectDeletesTheToken() {
        FakeHttpTransport transport;
        FakeSecretStore secrets;
        secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        GitHubClient client(transport, QStringLiteral("1.0"));
        GitHubAccount account(secrets, client);
        QVERIFY(wait<SecretWrite>([&](auto done) { account.disconnect(nullptr, done); }));
        QVERIFY(!secrets.values.contains(QStringLiteral("token")));
    }

    // Without a token nothing is sent; with one, the year from the start of the local day 364 days ago up to now.
    void reportFetchesTheYearUpToNow() {
        FakeHttpTransport transport;
        transport.respond(200, fixture("calendar"));
        FakeSecretStore secrets;
        GitHubClient client(transport, QStringLiteral("1.0"));
        GitHubAccount account(secrets, client);
        const QDateTime now(QDate(2026, 9, 29), QTime(15, 0), QTimeZone("Asia/Tokyo"));

        const auto missing = wait<ReportOutcome>([&](auto done) { account.report(now, Qt::Monday, nullptr, done); });
        QVERIFY(!missing);
        QCOMPARE(missing.error().kind, SourceError::Kind::TokenMissing);
        QVERIFY(transport.requests.isEmpty());

        secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        const auto report = wait<ReportOutcome>([&](auto done) { account.report(now, Qt::Monday, nullptr, done); });
        QVERIFY(report);
        const QJsonObject variables = QJsonDocument::fromJson(transport.requests.first().body)
                                          .object()
                                          .value(QStringLiteral("variables"))
                                          .toObject();
        QCOMPARE(variables.value(QStringLiteral("from")).toString(), QStringLiteral("2025-09-30T00:00:00+09:00"));
        QCOMPARE(variables.value(QStringLiteral("to")).toString(), QStringLiteral("2026-09-29T15:00:00+09:00"));
        QCOMPARE(report->calendar.login, QStringLiteral("octocat"));
        QCOMPARE(report->stats.today, 28);
    }
};

QTEST_GUILESS_MAIN(TestGitHubAccount)
#include "tst_githubaccount.moc"
