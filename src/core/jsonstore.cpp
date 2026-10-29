#include "core/jsonstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>

namespace contribusage::jsonstore {

std::expected<std::optional<QJsonValue>, StoreError> read(const QString &file, int schemaVersion) {
    QFile input(file);
    if (!input.exists()) {
        return std::optional<QJsonValue>();
    }
    if (!input.open(QIODevice::ReadOnly)) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = input.errorString()});
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(input.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Malformed, .message = parseError.errorString()});
    }
    const QJsonObject envelope = document.object();
    const int version = envelope.value(QStringLiteral("schemaVersion")).toInt(-1);
    if (version != schemaVersion) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::UnsupportedSchemaVersion, .version = version});
    }
    return std::optional<QJsonValue>(envelope.value(QStringLiteral("value")));
}

std::expected<void, StoreError> write(const QString &file, int schemaVersion, const QJsonValue &value) {
    const QString folder = QFileInfo(file).absolutePath();
    if (!createFolder(folder)) {
        return std::unexpected(
            StoreError{.kind = StoreError::Kind::Io, .message = QStringLiteral("cannot create ") + folder});
    }
    QSaveFile output(file);
    if (!output.open(QIODevice::WriteOnly)) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = output.errorString()});
    }
    const QJsonObject envelope{{QStringLiteral("schemaVersion"), schemaVersion}, {QStringLiteral("value"), value}};
    output.write(QJsonDocument(envelope).toJson(QJsonDocument::Compact));
    if (!output.commit()) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Io, .message = output.errorString()});
    }
    return {};
}

bool createFolder(const QString &folder) {
    const QString path = QDir::cleanPath(QFileInfo(folder).absoluteFilePath());
    QStringList missing;
    QString current = path;
    while (!QFileInfo::exists(current)) {
        missing.prepend(current);
        const QString parent = QFileInfo(current).path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    for (const QString &directory : missing) {
        if (!QDir().mkdir(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
            return false;
        }
    }
    return QFileInfo(path).isDir();
}

bool deleteProviderData(const QString &providerId, const AppPaths &paths) {
    return QDir(paths.providerFolder(providerId)).removeRecursively();
}

} // namespace contribusage::jsonstore
