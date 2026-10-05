#include "popupmodel.h"

#include "claudecode/activityjob.h"
#include "claudecode/activityview.h"
#include "claudecode/claudecodelimits.h"
#include "claudecode/claudelocator.h"
#include "claudecode/limitsjob.h"
#include "claudecode/limitsview.h"
#include "core/apppaths.h"
#include "core/display.h"
#include "core/heatmap.h"
#include "core/process.h"
#include "core/version.h"
#include "github/githubaccount.h"
#include "github/githubclient.h"
#include "github/githubjob.h"
#include "github/githubview.h"
#include "platform/kwalletsecretstore.h"
#include "platform/networktransport.h"

#include <QDir>

#include <QLocale>

#include <algorithm>
#include <initializer_list>
#include <utility>

using namespace contribusage;

namespace {

QVariantMap window(const QString &label, double usedPercent, qint64 resetsIn, const QDateTime &now) {
    const std::optional<QDateTime> resetsAt = now.addSecs(resetsIn);
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("percentText"), percentText(usedPercent, false, resetsAt, now)},
        {QStringLiteral("fraction"), barFraction(usedPercent, resetsAt, now)},
        {QStringLiteral("level"), meterLevelName(meterLevel(usedPercent, resetsAt, now))},
        {QStringLiteral("resetText"), resetText(resetsAt, now).value_or(QString())},
    };
}

// The macOS preview's windows as /usage prints them, filtered and renamed for display (ADR-042).
QVariantList shownWindows(const QDateTime &now) {
    struct Printed {
        QString label;
        double usedPercent;
        qint64 resetsIn;
    };
    const QList<Printed> printed{{QStringLiteral("Current session"), 23, 7800},
                                 {QStringLiteral("Current week (all models)"), 74, 4 * 86400},
                                 {QStringLiteral("Current week (Opus)"), 93, 4 * 86400}};
    QVariantList windows;
    for (const Printed &entry : printed) {
        if (const auto label = shownWindowLabel(entry.label)) {
            windows.append(window(*label, entry.usedPercent, entry.resetsIn, now));
        }
    }
    return windows;
}

QVariantMap share(const QString &label, int percent) {
    return {{QStringLiteral("label"), label}, {QStringLiteral("percent"), QString(QString::number(percent) + u'%')}};
}

QVariantList shares(std::initializer_list<std::pair<QString, int>> items) {
    QVariantList list;
    for (const auto &[label, percent] : items) {
        list.append(share(label, percent));
    }
    return list;
}

QVariantMap insights() {
    const QVariantMap day{
        {QStringLiteral("label"), QStringLiteral("Last 24h")},
        {QStringLiteral("summary"), QStringLiteral("206 requests · 5 sessions")},
        {QStringLiteral("rows"), shares({{QStringLiteral("At >150k context"), 62}})},
        {QStringLiteral("rankings"),
         QVariantList{
             QVariantMap{{QStringLiteral("title"), QStringLiteral("Skills")},
                         {QStringLiteral("rows"), shares({{QStringLiteral("/skill-a"), 35},
                                                          {QStringLiteral("/skill-b"), 23},
                                                          {QStringLiteral("/skill-c"), 21}})}},
             QVariantMap{{QStringLiteral("title"), QStringLiteral("Plugins")},
                         {QStringLiteral("rows"), shares({{QStringLiteral("plugin-a"), 56}})}},
         }},
    };
    const QVariantMap week{
        {QStringLiteral("label"), QStringLiteral("Last 7d")},
        {QStringLiteral("summary"), QStringLiteral("3350 requests · 88 sessions")},
        {QStringLiteral("rows"),
         shares({{QStringLiteral("At >150k context"), 54}, {QStringLiteral("Subagent-heavy sessions"), 13}})},
        {QStringLiteral("rankings"),
         QVariantList{QVariantMap{{QStringLiteral("title"), QStringLiteral("Skills")},
                                  {QStringLiteral("rows"), shares({{QStringLiteral("/skill-a"), 42}})}}}},
    };
    return {{QStringLiteral("note"), QStringLiteral("Approximate, based on local sessions on this machine.")},
            {QStringLiteral("periods"), QVariantList{day, week}}};
}

// The macOS preview's tokens per day for the last 7 days (cache read in millions, plus input, output and cache write),
// and a repeating pattern for the 113 days before them, so the heatmap has history. Older days have no data.
QHash<QDate, qint64> mockTokens(QDate today) {
    static constexpr double lastWeek[] = {1.1, 3.4, 2.0, 5.1, 4.4, 0.8, 4.2};
    constexpr qint64 otherTokens = 12'000 + 812'000 + 300'000;
    QHash<QDate, qint64> tokens;
    for (int index = 0; index < 120; ++index) {
        const QDate day = today.addDays(index - 119);
        const int fromEnd = 119 - index;
        if (fromEnd < 7) {
            tokens.insert(day, qint64(lastWeek[6 - fromEnd] * 1'000'000) + otherTokens);
        } else {
            const int step = (index * 37 + index / 5) % 11;
            tokens.insert(day, step == 0 ? 0 : step * 500'000);
        }
    }
    return tokens;
}

// The macOS preview's GitHub calendar: 26 weeks of a fixed pattern ending today.
QHash<QDate, int> mockContributions(QDate today) {
    QHash<QDate, int> counts;
    for (int index = 0; index < 182; ++index) {
        counts.insert(today.addDays(index - 181), (index * 7 + index / 3) % 9);
    }
    return counts;
}

} // namespace

void PopupModel::buildFakeData() {
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();
    const auto tokens = mockTokens(today);

    QList<qint64> week;
    for (int offset = 6; offset >= 0; --offset) {
        week.append(tokens.value(today.addDays(-offset)));
    }
    QVariantList weekBars;
    const auto fractions = barFractions(week);
    for (qsizetype index = 0; index < fractions.size(); ++index) {
        weekBars.append(QVariantMap{{QStringLiteral("fraction"), fractions.at(index)},
                                    {QStringLiteral("today"), index == fractions.size() - 1}});
    }
    const QVariantMap activity{
        {QStringLiteral("todayText"),
         QString(QStringLiteral("Today: 668 requests · 8 sessions · ") + compactCount(tokens.value(today)))},
        {QStringLiteral("categoriesText"),
         QString(QStringLiteral("Input ") + compactCount(12'000) + QStringLiteral(" · Output ") +
                 compactCount(812'000) + QStringLiteral(" · Cache read ") + compactCount(4'200'000) +
                 QStringLiteral(" · Cache write ") + compactCount(300'000))},
        {QStringLiteral("week"), weekBars},
    };
    m_providers.append(QVariantMap{
        {QStringLiteral("name"), QStringLiteral("Claude Code")},
        {QStringLiteral("symbol"), QStringLiteral("draw-star")},
        {QStringLiteral("updatedText"), ageText(now.addSecs(-120), now)},
        {QStringLiteral("windows"), shownWindows(now)},
        {QStringLiteral("activity"), activity},
        {QStringLiteral("insights"), insights()},
    });

    const auto days = heatmapDays(today);
    QHash<QDate, qint64> shownTokens;
    for (const QDate &day : days) {
        if (tokens.contains(day)) {
            shownTokens.insert(day, tokens.value(day));
        }
    }
    const auto claudeLevels = quartileLevels(shownTokens);
    const auto contributions = mockContributions(today);
    QVariantList cells;
    for (const QDate &day : days) {
        QVariantList stripes;
        const int claudeLevel = claudeLevels.value(day);
        if (claudeLevel > 0) {
            stripes.append(
                QVariantMap{{QStringLiteral("hue"), QStringLiteral("claude")}, {QStringLiteral("level"), claudeLevel}});
        }
        const int count = contributions.value(day);
        if (count > 0) {
            stripes.append(QVariantMap{{QStringLiteral("hue"), QStringLiteral("github")},
                                       {QStringLiteral("level"), std::min(count / 2, 4)}});
        }
        const QString claudeText = shownTokens.contains(day)
                                       ? compactCount(shownTokens.value(day)) + QStringLiteral(" tokens")
                                       : QStringLiteral("no data");
        const QString githubText = QString::number(count) + QStringLiteral(" contributions");
        cells.append(QVariantMap{
            {QStringLiteral("line"), QString(day.toString(Qt::ISODate) + QStringLiteral(" · Claude Code: ") +
                                             claudeText + QStringLiteral(" · GitHub: ") + githubText)},
            {QStringLiteral("stripes"), stripes},
        });
    }
    m_heatmap = {
        {QStringLiteral("layers"), QVariantList{QVariantMap{{QStringLiteral("name"), QStringLiteral("Claude Code")},
                                                            {QStringLiteral("hue"), QStringLiteral("claude")}},
                                                QVariantMap{{QStringLiteral("name"), QStringLiteral("GitHub")},
                                                            {QStringLiteral("hue"), QStringLiteral("github")}}}},
        {QStringLiteral("columns"), int((days.size() + 6) / 7)},
        {QStringLiteral("cells"), cells},
    };

    m_github = {
        {QStringLiteral("login"), QStringLiteral("@octocat")},
        {QStringLiteral("statsText"),
         QString(QStringLiteral("Today 5 · Streak 12 days · Year ") + QLocale().toString(1234))},
        {QStringLiteral("updatedText"), ageText(now.addSecs(-120), now)},
    };
    m_label = {
        {QStringLiteral("text"), percentText(23, false, now.addSecs(7800), now)},
        {QStringLiteral("fraction"), barFraction(23, now.addSecs(7800), now)},
        {QStringLiteral("level"), meterLevelName(meterLevel(23, now.addSecs(7800), now))},
    };
}

PopupModel::PopupModel(QObject *parent) : QObject(parent) {
    m_fake = qgetenv("CONTRIBUSAGE_FAKE_DATA") == "1";
    if (m_fake) {
        buildFakeData();
        return;
    }
    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
    m_runner = std::make_unique<LiveProcessRunner>();
    m_locator = std::make_unique<claudecode::ClaudeLocator>(*m_runner, QDir::homePath(), shell);
    m_limits = std::make_unique<claudecode::ClaudeCodeLimits>(*m_runner, *m_locator, AppPaths::live());
    m_job = std::make_unique<claudecode::LimitsJob>(*m_limits, AppPaths::live(), 5 * 60 * 1000); // ADR-039
    connect(m_job.get(), &claudecode::LimitsJob::stateChanged, this, &PopupModel::changed);
    m_minute.setInterval(60 * 1000);
    connect(&m_minute, &QTimer::timeout, this, &PopupModel::changed);
    m_minute.start();
    m_job->start();
    m_activityJob =
        std::make_unique<claudecode::ActivityJob>(*m_runner, QDir::homePath(), shell, m_limits->probeFolder(),
                                                  AppPaths::live(), 5'000); // SPEC §8.3, 5 s debounce
    connect(m_activityJob.get(), &claudecode::ActivityJob::stateChanged, this, &PopupModel::changed);
    m_activityJob->start();
    m_transport = std::make_unique<NetworkTransport>();
    m_secrets = std::make_unique<KWalletSecretStore>();
    m_githubClient = std::make_unique<github::GitHubClient>(*m_transport, version());
    m_githubAccount = std::make_unique<github::GitHubAccount>(*m_secrets, *m_githubClient);
    m_githubJob = std::make_unique<github::GitHubJob>(*m_githubAccount, AppPaths::live(), 30 * 60 * 1000); // SPEC §12
    connect(m_githubJob.get(), &github::GitHubJob::stateChanged, this, &PopupModel::changed);
    m_githubJob->start();
}

// Out of line, where the owned types are complete; the job goes first, so no probe outlives its limits object.
PopupModel::~PopupModel() {
    m_activityJob.reset();
    m_githubJob.reset();
    m_job.reset();
}

QVariantMap PopupModel::heatmap() const {
    if (m_fake) {
        return m_heatmap;
    }
    const QDate today = QDate::currentDate();
    QList<HeatmapLayer> layers;
    for (const auto &layer :
         {claudecode::activityLayer(m_activityJob->state(), today), github::githubLayer(m_githubJob->state())}) {
        if (layer) {
            layers.append(*layer);
        }
    }
    return heatmapView(layers, today);
}

QVariantMap PopupModel::github() const {
    if (m_fake) {
        return m_github;
    }
    QVariantMap view = github::githubView(m_githubJob->state(), QDateTime::currentDateTime());
    view.insert(QStringLiteral("busy"), m_githubBusy);
    view.insert(QStringLiteral("connectError"), m_githubConnectError);
    view.insert(QStringLiteral("canDisconnect"), true);
    return view;
}

void PopupModel::connectGitHub(const QString &token) {
    if (m_fake || m_githubBusy) {
        return;
    }
    m_githubBusy = true;
    m_githubConnectError.clear();
    Q_EMIT changed();
    m_githubAccount->connect(token, this, [this](std::expected<QString, SourceError> login) {
        m_githubBusy = false;
        if (login) {
            m_githubJob->tokenChanged();
        } else {
            m_githubConnectError = errorMessage(login.error(), QStringLiteral("GitHub"));
        }
        Q_EMIT changed();
    });
}

void PopupModel::disconnectGitHub() {
    if (m_fake) {
        return;
    }
    m_githubJob->disconnectAccount();
}

QVariantList PopupModel::providers() const {
    if (m_fake) {
        return m_providers;
    }
    QVariantMap provider = claudecode::limitsView(m_job->state(), QDateTime::currentDateTime());
    const QVariantMap activity = claudecode::activityView(m_activityJob->state(), QDate::currentDate());
    if (!activity.isEmpty()) {
        provider.insert(QStringLiteral("activity"), activity);
    }
    return {provider};
}

QVariantMap PopupModel::label() const {
    return m_fake ? m_label : claudecode::labelView(m_job->state(), QDateTime::currentDateTime());
}
