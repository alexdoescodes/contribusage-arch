#include "platform/kwalletsecretstore.h"

#include <KWallet>

namespace contribusage {

namespace {
const QString folder = QStringLiteral("contribusage");

SourceError unavailable() {
    return {.kind = SourceError::Kind::Io, .text = QStringLiteral("KWallet is not available")};
}
} // namespace

KWalletSecretStore::KWalletSecretStore(QObject *parent) : QObject(parent) {}

KWalletSecretStore::~KWalletSecretStore() = default;

void KWalletSecretStore::withWallet(QObject *context, std::function<void(KWallet::Wallet *)> action) {
    if (m_wallet && m_wallet->isOpen()) {
        action(m_wallet.get());
        return;
    }
    m_waiting.append({.context = context, .hasContext = context != nullptr, .action = std::move(action)});
    if (m_opening) {
        return;
    }
    m_opening = true;
    m_wallet.reset(KWallet::Wallet::openWallet(KWallet::Wallet::NetworkWallet(), 0, KWallet::Wallet::Asynchronous));
    if (!m_wallet) {
        flush(false);
        return;
    }
    connect(m_wallet.get(), &KWallet::Wallet::walletOpened, this, [this](bool opened) { flush(opened); });
}

void KWalletSecretStore::flush(bool opened) {
    m_opening = false;
    KWallet::Wallet *wallet = nullptr;
    if (opened && m_wallet) {
        if (!m_wallet->hasFolder(folder)) {
            m_wallet->createFolder(folder);
        }
        if (m_wallet->setFolder(folder)) {
            wallet = m_wallet.get();
        }
    }
    const QList<Waiting> waiting = std::exchange(m_waiting, {});
    for (const Waiting &entry : waiting) {
        if (!entry.hasContext || entry.context) {
            entry.action(wallet);
        }
    }
    if (!wallet && m_wallet) {
        m_wallet.release()->deleteLater(); // try again on the next request
    }
}

void KWalletSecretStore::read(const QString &key, QObject *context, std::function<void(SecretRead)> done) {
    withWallet(context, [key, done = std::move(done)](KWallet::Wallet *wallet) {
        if (!wallet) {
            done(std::unexpected(unavailable()));
            return;
        }
        if (!wallet->hasEntry(key)) {
            done(std::optional<QString>());
            return;
        }
        QString value;
        if (wallet->readPassword(key, value) != 0) {
            done(std::unexpected(unavailable()));
            return;
        }
        done(std::optional<QString>(value));
    });
}

void KWalletSecretStore::write(const QString &key, const QString &value, QObject *context,
                               std::function<void(SecretWrite)> done) {
    withWallet(context, [key, value, done = std::move(done)](KWallet::Wallet *wallet) {
        if (!wallet || wallet->writePassword(key, value) != 0) {
            done(std::unexpected(unavailable()));
            return;
        }
        done({});
    });
}

void KWalletSecretStore::remove(const QString &key, QObject *context, std::function<void(SecretWrite)> done) {
    withWallet(context, [key, done = std::move(done)](KWallet::Wallet *wallet) {
        if (!wallet) {
            done(std::unexpected(unavailable()));
            return;
        }
        if (wallet->hasEntry(key) && wallet->removeEntry(key) != 0) {
            done(std::unexpected(unavailable()));
            return;
        }
        done({});
    });
}

} // namespace contribusage
