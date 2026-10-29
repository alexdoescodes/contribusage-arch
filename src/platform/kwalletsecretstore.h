#pragma once

#include "core/seams.h"

#include <QList>
#include <QObject>
#include <QPointer>

#include <memory>

namespace KWallet {
class Wallet;
}

namespace contribusage {

// SecretStore over KWallet, which KF6 serves through the Secret Service API (port design §1). Entries live in the
// network wallet's `contribusage` folder. The wallet opens asynchronously on first use; requests wait for it, and fail
// with Io when it is unavailable.
class KWalletSecretStore final : public QObject, public SecretStore {
public:
    explicit KWalletSecretStore(QObject *parent = nullptr);
    ~KWalletSecretStore() override;

    void read(const QString &key, QObject *context, std::function<void(SecretRead)> done) override;
    void write(const QString &key, const QString &value, QObject *context,
               std::function<void(SecretWrite)> done) override;
    void remove(const QString &key, QObject *context, std::function<void(SecretWrite)> done) override;

private:
    // Runs `action` with the open wallet in the folder, or with nullptr when the wallet is unavailable.
    void withWallet(QObject *context, std::function<void(KWallet::Wallet *)> action);
    void flush(bool opened);

    struct Waiting {
        QPointer<QObject> context;
        bool hasContext;
        std::function<void(KWallet::Wallet *)> action;
    };
    std::unique_ptr<KWallet::Wallet> m_wallet;
    bool m_opening = false;
    QList<Waiting> m_waiting;
};

} // namespace contribusage
