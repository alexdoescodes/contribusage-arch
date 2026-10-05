#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>

namespace contribusage {
class LiveProcessRunner;
class ScheduleConditionsSource;
class SystemConditions;
class DesktopNotifier;
class NetworkTransport;
class KWalletSecretStore;
namespace github {
class GitHubClient;
class GitHubAccount;
class GitHubJob;
} // namespace github
namespace claudecode {
class ActivityJob;
class ClaudeLocator;
class ClaudeCodeLimits;
class LimitsJob;
} // namespace claudecode
} // namespace contribusage

// What the popup and the panel label show. Live mode runs the Claude Code limits probe (L-2a); blocks without a data
// source yet are left empty so QML hides them. CONTRIBUSAGE_FAKE_DATA=1 shows the macOS app's preview data instead
// (ADR-036's debug flag), which the applet tests use.
class PopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList providers READ providers NOTIFY changed)
    Q_PROPERTY(QVariantMap heatmap READ heatmap NOTIFY changed)
    Q_PROPERTY(QVariantMap github READ github NOTIFY changed)
    Q_PROPERTY(QVariantMap label READ label NOTIFY changed)
    Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)

public:
    explicit PopupModel(QObject *parent = nullptr);
    ~PopupModel() override;

    QVariantList providers() const;
    QVariantMap heatmap() const;
    QVariantMap github() const;
    QVariantMap label() const;
    QString dataFolder() const;
    Q_INVOKABLE void applySettings(const QVariantMap &configuration);
    Q_INVOKABLE void deleteClaudeCodeData();
    Q_INVOKABLE void resetCaches();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void popupOpened();
    Q_INVOKABLE void connectGitHub(const QString &token);
    Q_INVOKABLE void disconnectGitHub();

Q_SIGNALS:
    void changed();

private:
    void buildFakeData();

    bool m_fake = false;
    QVariantList m_providers; // fake mode
    QVariantMap m_heatmap;    // fake mode
    QVariantMap m_github;     // fake mode
    QVariantMap m_label;      // fake mode
    std::unique_ptr<contribusage::ScheduleConditionsSource> m_conditions;
    std::unique_ptr<contribusage::SystemConditions> m_system;
    std::unique_ptr<contribusage::DesktopNotifier> m_notifier;
    int m_menuBarMode = 0; // MenuBarMode
    std::unique_ptr<contribusage::LiveProcessRunner> m_runner;
    std::unique_ptr<contribusage::claudecode::ClaudeLocator> m_locator;
    std::unique_ptr<contribusage::claudecode::ClaudeCodeLimits> m_limits;
    std::unique_ptr<contribusage::claudecode::LimitsJob> m_job;
    std::unique_ptr<contribusage::claudecode::ActivityJob> m_activityJob;
    std::unique_ptr<contribusage::NetworkTransport> m_transport;
    std::unique_ptr<contribusage::KWalletSecretStore> m_secrets;
    std::unique_ptr<contribusage::github::GitHubClient> m_githubClient;
    std::unique_ptr<contribusage::github::GitHubAccount> m_githubAccount;
    std::unique_ptr<contribusage::github::GitHubJob> m_githubJob;
    bool m_githubBusy = false;
    QString m_githubConnectError;
    QTimer m_minute; // ages and countdowns move on (SPEC §11.2)
};
