#pragma once

#include "core/reports.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace contribusage {

// Everything the UI shows is a snapshot (SPEC §5, §10.1).
template <typename T> struct Snapshot {
    T value;
    QDateTime fetchedAt;
    QString origin;

    // SPEC §12 "stale after".
    bool isStale(const QDateTime &now, qint64 staleAfterSeconds) const {
        return fetchedAt.secsTo(now) > staleAfterSeconds;
    }
    bool operator==(const Snapshot &) const = default;
};

// An open set of origins; providers add their own.
namespace origins {
inline QString cache() {
    return QStringLiteral("cache");
}
inline QString poll() {
    return QStringLiteral("poll");
}
inline QString push() {
    return QStringLiteral("push");
}
inline QString activity() {
    return QStringLiteral("activity");
}
inline QString github() {
    return QStringLiteral("github");
}
} // namespace origins

enum class NotConfiguredReason { ProviderDisabled, ToolNotInstalled, UnsupportedPlan, NoLocalData, GithubTokenMissing };

// SPEC §11.3: a source's state. While loading or failed it keeps the previous snapshot, so the UI can show it dimmed.
template <typename T> struct SourceState {
    enum class Phase { NotConfigured, Loading, Loaded, Failed };

    Phase phase = Phase::Loading;
    NotConfiguredReason reason = NotConfiguredReason::ProviderDisabled;
    QString note;                       // UnsupportedPlan: the tool's own text (ADR-017)
    std::optional<SourceError> error;   // Failed
    std::optional<Snapshot<T>> current; // Loaded: the value; Loading and Failed: the previous one

    static SourceState notConfigured(NotConfiguredReason reason, const QString &note = {}) {
        return {.phase = Phase::NotConfigured, .reason = reason, .note = note, .error = {}, .current = {}};
    }
    static SourceState loading(std::optional<Snapshot<T>> previous) {
        return {.phase = Phase::Loading, .reason = {}, .note = {}, .error = {}, .current = std::move(previous)};
    }
    static SourceState loaded(Snapshot<T> snapshot) {
        return {.phase = Phase::Loaded, .reason = {}, .note = {}, .error = {}, .current = std::move(snapshot)};
    }
    static SourceState failed(SourceError error, std::optional<Snapshot<T>> previous) {
        return {.phase = Phase::Failed,
                .reason = {},
                .note = {},
                .error = std::move(error),
                .current = std::move(previous)};
    }

    // The values the UI can show, current or previous.
    std::optional<Snapshot<T>> snapshot() const { return phase == Phase::NotConfigured ? std::nullopt : current; }
};

} // namespace contribusage
