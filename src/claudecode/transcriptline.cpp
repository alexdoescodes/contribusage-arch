#include "claudecode/transcriptline.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace contribusage::claudecode {

QString TranscriptLine::key() const {
    if (messageId.isEmpty()) {
        return {};
    }
    return requestId.isEmpty() ? messageId : QString(messageId + u':' + requestId);
}

std::expected<std::optional<TranscriptLine>, QString> decodeTranscriptLine(QByteArrayView line) {
    if (line.indexOf(QByteArrayView("\"usage\"")) < 0) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromRawData(line.data(), line.size()), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::unexpected(QStringLiteral("not JSON"));
    }
    const QJsonObject object = document.object();
    const QJsonObject message = object.value(QStringLiteral("message")).toObject();
    const QJsonValue usageValue = message.value(QStringLiteral("usage"));
    const QString model = message.value(QStringLiteral("model")).toString();
    if (object.value(QStringLiteral("type")).toString() != QStringLiteral("assistant") || !usageValue.isObject() ||
        model.isEmpty() || (model.startsWith(u'<') && model.endsWith(u'>'))) {
        return std::nullopt;
    }
    const QDateTime timestamp =
        QDateTime::fromString(object.value(QStringLiteral("timestamp")).toString(), Qt::ISODateWithMs);
    if (!timestamp.isValid()) {
        return std::unexpected(QStringLiteral("no timestamp"));
    }
    const QJsonObject usage = usageValue.toObject();
    return TranscriptLine{
        .timestamp = timestamp,
        .sessionId = object.value(QStringLiteral("sessionId")).toString(),
        .requestId = object.value(QStringLiteral("requestId")).toString(),
        .messageId = message.value(QStringLiteral("id")).toString(),
        .model = model,
        .isSidechain = object.value(QStringLiteral("isSidechain")).toBool(),
        .tokens = {.input = usage.value(QStringLiteral("input_tokens")).toInteger(),
                   .output = usage.value(QStringLiteral("output_tokens")).toInteger(),
                   .cacheWrite = usage.value(QStringLiteral("cache_creation_input_tokens")).toInteger(),
                   .cacheRead = usage.value(QStringLiteral("cache_read_input_tokens")).toInteger()}};
}

} // namespace contribusage::claudecode
