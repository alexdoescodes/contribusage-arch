#include "github/githubjob.h"

#include "core/jsonstore.h"
#include "core/limitscache.h"

#include <QJsonObject>
#include <QLocale>

namespace contribusage::github {

namespace githubcache {

namespace {
QString file(const AppPaths &paths) {
    return paths.root + QStringLiteral("/state.json");
}

QJsonObject state(const AppPaths &paths) {
    const auto read = jsonstore::read(file(paths), limitscache::schemaVersion);
    return read && read->has_value() ? read->value().toObject() : QJsonObject();
}
} // namespace

std::optional<Snapshot<GitHubReport>> load(const AppPaths &paths) {
    const QJsonObject entry = state(paths).value(QStringLiteral("github")).toObject();
    const QDateTime fetchedAt =
        QDateTime::fromString(entry.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODateWithMs);
    const auto report = gitHubReportFromJson(entry.value(QStringLiteral("report")).toObject());
    if (!fetchedAt.isValid() || !report) {
        return std::nullopt;
    }
    return Snapshot<GitHubReport>{
        .value = *report, .fetchedAt = fetchedAt, .origin = entry.value(QStringLiteral("origin")).toString()};
}

bool clear(const AppPaths &paths) {
    QJsonObject root = state(paths);
    root.remove(QStringLiteral("github"));
    return jsonstore::write(file(paths), limitscache::schemaVersion, root).has_value();
}

bool save(const AppPaths &paths, const Snapshot<GitHubReport> &snapshot) {
    QJsonObject root = state(paths);
    root.insert(QStringLiteral("github"),
                QJsonObject{{QStringLiteral("fetchedAt"), snapshot.fetchedAt.toUTC().toString(Qt::ISODateWithMs)},
                            {QStringLiteral("origin"), snapshot.origin},
                            {QStringLiteral("report"), toJson(snapshot.value)}});
    return jsonstore::write(file(paths), limitscache::schemaVersion, root).has_value();
}

} // namespace githubcache

GitHubJob::GitHubJob(GitHubAccount &account, AppPaths paths, int intervalMs, QObject *parent)
    : QObject(parent), m_account(account), m_paths(std::move(paths)), m_interval(intervalMs) {
    m_timer.setInterval(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &GitHubJob::refresh);
}

void GitHubJob::start() {
    m_state = SourceState<GitHubReport>::loading(githubcache::load(m_paths));
    Q_EMIT stateChanged();
    refresh();
    m_timer.start();
}

void GitHubJob::refresh() {
    if (m_fetching || m_paused || (m_waitUntil.isValid() && QDateTime::currentDateTimeUtc() < m_waitUntil)) {
        return;
    }
    m_fetching = true;
    m_account.report(QDateTime::currentDateTime(), QLocale().firstDayOfWeek(), this,
                     [this, generation = m_generation](std::expected<GitHubReport, SourceError> outcome) {
                         if (generation == m_generation) {
                             finish(std::move(outcome));
                         }
                     });
}

void GitHubJob::tokenChanged() {
    ++m_generation;
    m_fetching = false;
    m_paused = false;
    m_waitUntil = {};
    githubcache::clear(m_paths);
    m_state = SourceState<GitHubReport>::loading(std::nullopt);
    Q_EMIT stateChanged();
    refresh();
}

void GitHubJob::disconnectAccount() {
    m_account.disconnect(this, [this](SecretWrite removed) {
        if (removed) {
            tokenChanged();
            return;
        }
        m_state = SourceState<GitHubReport>::failed(removed.error(), m_state.snapshot());
        Q_EMIT stateChanged();
    });
}

void GitHubJob::finish(std::expected<GitHubReport, SourceError> outcome) {
    m_fetching = false;
    if (outcome) {
        const Snapshot<GitHubReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::github()};
        githubcache::save(m_paths, snapshot);
        m_state = SourceState<GitHubReport>::loaded(snapshot);
    } else if (outcome.error().kind == SourceError::Kind::TokenMissing) {
        m_paused = true;
        m_state = SourceState<GitHubReport>::notConfigured(NotConfiguredReason::GithubTokenMissing);
    } else {
        // ADR-022; Io comes only from the secret store here: a denied or missing wallet is not asked again.
        if (outcome.error().kind == SourceError::Kind::Unauthorized || outcome.error().kind == SourceError::Kind::Io) {
            m_paused = true;
        } else if (outcome.error().kind == SourceError::Kind::RateLimited) {
            m_waitUntil = outcome.error().until; // ADR-020
        }
        m_state = SourceState<GitHubReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}

} // namespace contribusage::github
