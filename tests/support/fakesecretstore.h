#pragma once

#include "core/seams.h"

#include <QMap>
#include <QPointer>
#include <QTimer>

// An in-memory SecretStore; answers on the next event loop pass, like the wallet.
class FakeSecretStore final : public contribusage::SecretStore {
public:
    void read(const QString &key, QObject *context, std::function<void(contribusage::SecretRead)> done) override {
        ++reads;
        const contribusage::SecretRead outcome = failure ? contribusage::SecretRead(std::unexpected(*failure))
                                                 : values.contains(key) ? std::optional<QString>(values.value(key))
                                                                        : std::optional<QString>();
        later(context, [outcome, done = std::move(done)] { done(outcome); });
    }
    void write(const QString &key, const QString &value, QObject *context,
               std::function<void(contribusage::SecretWrite)> done) override {
        values.insert(key, value);
        later(context, [done = std::move(done)] { done({}); });
    }
    void remove(const QString &key, QObject *context, std::function<void(contribusage::SecretWrite)> done) override {
        const contribusage::SecretWrite outcome =
            failure ? contribusage::SecretWrite(std::unexpected(*failure)) : contribusage::SecretWrite();
        if (!failure) {
            values.remove(key);
        }
        later(context, [outcome, done = std::move(done)] { done(outcome); });
    }

    QMap<QString, QString> values;
    std::optional<contribusage::SourceError> failure; // read and remove answer with it (KWallet unavailable or denied)
    int reads = 0;

private:
    static void later(QObject *context, std::function<void()> action) {
        const QPointer<QObject> guard = context;
        const bool hasContext = context != nullptr;
        QTimer::singleShot(0, [guard, hasContext, action = std::move(action)]() {
            if (!hasContext || guard) {
                action();
            }
        });
    }
};
