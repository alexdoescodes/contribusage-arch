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

GitHubJob::GitHubJob(GitHubAccount &account, AppPaths paths, SchedulePolicy policy,
                     ScheduleConditionsSource *conditions, QObject *parent)
    : QObject(parent), m_account(account), m_paths(std::move(paths)), m_schedule(policy, conditions) {
    connect(&m_schedule, &SourceSchedule::due, this, &GitHubJob::run);
}

SchedulePolicy GitHubJob::defaultPolicy() {
    return {.defaultMs = 30 * 60'000,
            .minimumMs = 10 * 60'000,
            .maximumMs = 6 * 3600'000,
            .staleAfterMs = 2 * 3600'000,
            .manualFloorMs = 30'000,
            .needsNetwork = true};
}

void GitHubJob::start() {
    m_state = SourceState<GitHubReport>::loading(githubcache::load(m_paths));
    Q_EMIT stateChanged();
    m_schedule.start();
}

void GitHubJob::refresh() {
    m_schedule.requestManual();
}

void GitHubJob::popupOpened() {
    const auto snapshot = m_state.snapshot();
    if (!snapshot || snapshot->fetchedAt.msecsTo(QDateTime::currentDateTimeUtc()) > 10 * 60'000) {
        m_schedule.triggerIfDue();
    }
}

void GitHubJob::run() {
    m_account.report(QDateTime::currentDateTime(), QLocale().firstDayOfWeek(), this,
                     [this, generation = m_generation](std::expected<GitHubReport, SourceError> outcome) {
                         if (generation == m_generation) {
                             finish(std::move(outcome));
                         }
                     });
}

void GitHubJob::tokenChanged() {
    ++m_generation;
    githubcache::clear(m_paths);
    m_state = SourceState<GitHubReport>::loading(std::nullopt);
    Q_EMIT stateChanged();
    m_schedule.reset();
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
    m_schedule.finished(outcome.has_value());
    if (outcome) {
        const Snapshot<GitHubReport> snapshot{
            .value = std::move(*outcome), .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::github()};
        githubcache::save(m_paths, snapshot);
        m_state = SourceState<GitHubReport>::loaded(snapshot);
        m_schedule.setPaused(false);
    } else if (outcome.error().kind == SourceError::Kind::TokenMissing) {
        m_schedule.setPaused(true);
        m_state = SourceState<GitHubReport>::notConfigured(NotConfiguredReason::GithubTokenMissing);
    } else {
        // ADR-022; Io comes only from the secret store here: a denied or missing wallet is not asked again.
        if (outcome.error().kind == SourceError::Kind::Unauthorized || outcome.error().kind == SourceError::Kind::Io) {
            m_schedule.setPaused(true);
        } else if (outcome.error().kind == SourceError::Kind::RateLimited) {
            m_schedule.holdUntil(outcome.error().until); // ADR-020
        }
        m_state = SourceState<GitHubReport>::failed(outcome.error(), m_state.snapshot());
    }
    Q_EMIT stateChanged();
}

} // namespace contribusage::github
