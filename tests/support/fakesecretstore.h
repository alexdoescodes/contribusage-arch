#pragma once

#include "core/seams.h"

#include <QMap>
#include <QPointer>
#include <QTimer>

// An in-memory SecretStore; answers on the next event loop pass, like the wallet.
class FakeSecretStore final : public contribusage::SecretStore {
public:
    void read(const QString &key, QObject *context, std::function<void(contribusage::SecretRead)> done) override {
        const contribusage::SecretRead outcome =
            values.contains(key) ? std::optional<QString>(values.value(key)) : std::optional<QString>();
        later(context, [outcome, done = std::move(done)] { done(outcome); });
    }
    void write(const QString &key, const QString &value, QObject *context,
               std::function<void(contribusage::SecretWrite)> done) override {
        values.insert(key, value);
        later(context, [done = std::move(done)] { done({}); });
    }
    void remove(const QString &key, QObject *context, std::function<void(contribusage::SecretWrite)> done) override {
        values.remove(key);
        later(context, [done = std::move(done)] { done({}); });
    }

    QMap<QString, QString> values;

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
