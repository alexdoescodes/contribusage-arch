#pragma once

#include "core/apppaths.h"

#include <QJsonValue>
#include <QString>

#include <expected>
#include <optional>

namespace contribusage {

// Why a data file could not be read or written (SPEC §10.7, ADR-005).
struct StoreError {
    enum class Kind { UnsupportedSchemaVersion, Malformed, Io };
    Kind kind;
    int version = 0; // UnsupportedSchemaVersion: the version the file holds
    QString message = {};

    bool operator==(const StoreError &) const = default;
};

namespace jsonstore {

// The `value` of `{"schemaVersion": n, "value": …}`, or nullopt when the file does not exist. A file of another version
// or a broken file is an error and stays untouched: a cache's owner overwrites it on the next write, history.json's
// owner backs it up and migrates it.
std::expected<std::optional<QJsonValue>, StoreError> read(const QString &file, int schemaVersion);

// Creates missing folders owner-only (0700), then replaces the file atomically.
std::expected<void, StoreError> write(const QString &file, int schemaVersion, const QJsonValue &value);

// Creates `folder` and its missing parents owner-only; an existing folder keeps its mode.
bool createFolder(const QString &folder);

// US-12: removes root/providers/<id> and nothing else; a missing folder is fine.
bool deleteProviderData(const QString &providerId, const AppPaths &paths);

} // namespace jsonstore
} // namespace contribusage
