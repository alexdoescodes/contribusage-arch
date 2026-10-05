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
#include "core/jsonstore.h"
#include "core/menubarlabel.h"
#include "core/notificationstore.h"
#include "core/process.h"
#include "core/sourceschedule.h"
#include "core/version.h"
#include "core/widgetsettings.h"
#include "github/githubaccount.h"
#include "github/githubclient.h"
#include "github/githubjob.h"
#include "github/githubview.h"
#include "platform/desktopnotifier.h"
#include "platform/kwalletsecretstore.h"
#include "platform/networktransport.h"
#include "platform/systemconditions.h"

#include <QDir>
#include <QFile>

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
        {QStringLiteral("stale"), false},
        {QStringLiteral("title"), QStringLiteral("Claude Code")},
        {QStringLiteral("companion"), 0.74}, // the preview's "Current week (all models)"
        {QStringLiteral("source"), QStringLiteral("claude-code")},
    };
}

PopupModel::PopupModel(QObject *parent) : QObject(parent) {
    m_fake = qgetenv("CONTRIBUSAGE_FAKE_DATA") == "1";
    if (m_fake) {
        buildFakeData();
        return;
    }
    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
    m_conditions = std::make_unique<ScheduleConditionsSource>();
    m_system = std::make_unique<SystemConditions>(*m_conditions);
    m_runner = std::make_unique<LiveProcessRunner>();
    m_locator = std::make_unique<claudecode::ClaudeLocator>(*m_runner, QDir::homePath(), shell);
    m_limits = std::make_unique<claudecode::ClaudeCodeLimits>(*m_runner, *m_locator, AppPaths::live());
    m_job = std::make_unique<claudecode::LimitsJob>(*m_limits, AppPaths::live(), claudecode::LimitsJob::defaultPolicy(),
                                                    m_conditions.get());
    m_notifier = std::make_unique<DesktopNotifier>();
    m_job->setNotifications(m_notifier.get(), {});
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
    m_githubJob = std::make_unique<github::GitHubJob>(*m_githubAccount, AppPaths::live(),
                                                      github::GitHubJob::defaultPolicy(), m_conditions.get());
    connect(m_githubJob.get(), &github::GitHubJob::stateChanged, this, &PopupModel::changed);
    m_githubJob->start();
}

// Out of line, where the owned types are complete; the job goes first, so no probe outlives its limits object.
PopupModel::~PopupModel() {
    m_activityJob.reset();
    m_githubJob.reset();
    m_job.reset();
    m_system.reset();
    m_conditions.reset();
}

QVariantMap PopupModel::heatmap() const {
    if (m_fake) {
        QVariantMap view = m_heatmap;
        view.insert(QStringLiteral("style"), m_layout.heatmapStyle);
        return view;
    }
    const QDate today = QDate::currentDate();
    // FR-48: the layers in the heatmap, in block order.
    QList<HeatmapLayer> layers;
    for (const QString &source : m_layout.heatmapLayers({QStringLiteral("claude-code")}, sourcesOn())) {
        const auto layer = source == githubBlockId()
                               ? github::githubLayer(m_githubJob->state(), QDateTime::currentDateTime())
                               : claudecode::activityLayer(m_activityJob->state(), today);
        if (layer) {
            layers.append(*layer);
        }
    }
    QVariantMap view = heatmapView(layers, today);
    if (!view.isEmpty()) {
        view.insert(QStringLiteral("style"), m_layout.heatmapStyle);
    }
    return view;
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

// FR-47, FR-2: which sources are on. Claude Code is the one provider and always on; GitHub with its switch.
QSet<QString> PopupModel::sourcesOn() const {
    QSet<QString> on{QStringLiteral("claude-code")};
    if (m_githubOn) {
        on.insert(githubBlockId());
    }
    return on;
}

QStringList PopupModel::blocks() const {
    return m_layout.blocks({QStringLiteral("claude-code")}, sourcesOn());
}

bool PopupModel::nothingToShow() const {
    return blocks().isEmpty();
}

// FR-46: activity runs while its section or layer shows or the label's heatmap styles draw it; GitHub likewise.
void PopupModel::applyDemand() {
    if (m_fake) {
        return;
    }
    QSet<QString> menuBar;
    if (m_menuBarStyle == QStringLiteral("heatmap") || m_menuBarStyle == QStringLiteral("sharedHeatmap")) {
        menuBar.insert(QStringLiteral("claude-code"));
    }
    if (m_menuBarStyle == QStringLiteral("sharedHeatmap") || m_menuBarMode == int(MenuBarMode::GithubToday) ||
        m_menuBarMode == int(MenuBarMode::PrimaryAndGitHub)) {
        menuBar.insert(githubBlockId());
    }
    const Demand demand = m_layout.demand(sourcesOn(), menuBar);
    m_activityJob->setActive(demand.activity.contains(QStringLiteral("claude-code")));
    m_githubJob->setEnabled(m_githubOn);     // the user's switch (FR-43)
    m_githubJob->setDemanded(demand.github); // FR-46, keeping the schedule's history
}

namespace {
const QStringList registeredProviders{QStringLiteral("claude-code")};

QString blockName(const QString &block) {
    return block == heatmapBlockId()  ? QStringLiteral("Heatmap")
           : block == githubBlockId() ? QStringLiteral("GitHub")
                                      : QStringLiteral("Claude Code");
}
} // namespace

QVariantList PopupModel::layoutRows(const QString &text) const {
    const PopoverLayout layout = popoverLayoutFromText(text);
    QVariantList rows;
    for (const QString &block : layout.arranged(registeredProviders)) {
        QVariantMap row{{QStringLiteral("id"), block},
                        {QStringLiteral("name"), blockName(block)},
                        {QStringLiteral("shown"), !layout.hiddenBlocks.contains(block)}};
        if (block != heatmapBlockId()) {
            row.insert(QStringLiteral("inHeatmap"), !layout.outOfHeatmap.contains(block));
        }
        if (registeredProviders.contains(block)) {
            QVariantList sections;
            for (const SectionKind kind : layout.sections(block)) {
                sections.append(QVariantMap{{QStringLiteral("name"), sectionName(kind)},
                                            {QStringLiteral("shown"), layout.shows(kind, block)}});
            }
            row.insert(QStringLiteral("sections"), sections);
        }
        rows.append(row);
    }
    return rows;
}

QString PopupModel::layoutMoveBlock(const QString &text, int from, int to) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    const QStringList blocks = layout.arranged(registeredProviders);
    if (from < 0 || to < 0 || from >= blocks.size() || to >= blocks.size()) {
        return text;
    }
    layout.move(blocks.at(from), blocks.at(to), registeredProviders);
    return popoverLayoutText(layout);
}

QString PopupModel::layoutSetShown(const QString &text, const QString &block, bool shown) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    if (shown) {
        layout.hiddenBlocks.remove(block);
    } else {
        layout.hiddenBlocks.insert(block);
    }
    return popoverLayoutText(layout);
}

QString PopupModel::layoutSetInHeatmap(const QString &text, const QString &block, bool in) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    if (in) {
        layout.outOfHeatmap.remove(block);
    } else {
        layout.outOfHeatmap.insert(block);
    }
    return popoverLayoutText(layout);
}

QString PopupModel::layoutSetSectionShown(const QString &text, const QString &provider, const QString &section,
                                          bool shown) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    if (const auto kind = sectionFromName(section)) {
        layout.setShows(*kind, provider, shown);
    }
    return popoverLayoutText(layout);
}

QString PopupModel::layoutMoveSection(const QString &text, const QString &provider, int from, int to) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    const QList<SectionKind> sections = layout.sections(provider);
    if (from < 0 || to < 0 || from >= sections.size() || to >= sections.size()) {
        return text;
    }
    layout.moveSection(sections.at(from), sections.at(to), provider);
    return popoverLayoutText(layout);
}

QString PopupModel::layoutSetHeatmapStyle(const QString &text, const QString &style) const {
    PopoverLayout layout = popoverLayoutFromText(text);
    layout.heatmapStyle = style == QStringLiteral("stacked") ? QStringLiteral("stacked") : QStringLiteral("combined");
    return popoverLayoutText(layout);
}

QString PopupModel::dataFolder() const {
    return AppPaths::live().root;
}

void PopupModel::applySettings(const QVariantMap &configuration) {
    const WidgetSettings settings = widgetSettingsFromMap(configuration);
    m_menuBarMode = int(settings.menuBarMode);
    m_githubOn = settings.githubEnabled;
    m_layout = popoverLayoutFromText(configuration.value(QStringLiteral("popoverLayout")).toString());
    m_menuBarStyle = configuration.value(QStringLiteral("menuBarStyle")).toString();
    if (!m_fake) {
        m_job->setInterval(settings.probeIntervalMinutes);
        m_job->setNotifications(m_notifier.get(), settings.notifications);
        m_limits->setOverride(settings.claudePathOverride);
        m_githubJob->setInterval(settings.githubIntervalMinutes);
    }
    applyDemand();
    Q_EMIT changed();
}

void PopupModel::deleteClaudeCodeData() {
    if (!m_fake) {
        // US-12: the cached limits, the notification cycles and the frozen history; each owner deletes its own.
        m_job->forgetData();
        m_activityJob->deleteHistory();
    }
}

void PopupModel::resetCaches() {
    if (m_fake) {
        return;
    }
    // ADR-028: state.json loses the caches, never the notification keys; history.json is untouched.
    const AppPaths paths = AppPaths::live();
    const NotificationsSent sent = notificationstore::load(paths);
    QFile::remove(paths.root + QStringLiteral("/state.json"));
    notificationstore::save(paths, sent);
    refresh();
}

void PopupModel::refresh() {
    if (m_fake) {
        return;
    }
    m_job->refresh();
    m_githubJob->refresh();
    m_activityJob->rescan();
}

void PopupModel::popupOpened() {
    if (m_fake) {
        return;
    }
    m_job->popupOpened();
    m_githubJob->popupOpened();
    m_activityJob->rescan(); // SPEC §12: transcripts on popup open
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
    // FR-44, ADR-034: the provider's shown sections in their order.
    QStringList sections;
    for (const SectionKind kind : m_layout.shownSections(QStringLiteral("claude-code"))) {
        sections.append(sectionName(kind));
    }
    if (m_fake) {
        QVariantList providers = m_providers;
        QVariantMap first = providers.first().toMap();
        first.insert(QStringLiteral("sections"), sections);
        providers[0] = first;
        return providers;
    }
    QVariantMap provider = claudecode::limitsView(m_job->state(), QDateTime::currentDateTime());
    const QVariantMap activity = claudecode::activityView(m_activityJob->state(), QDate::currentDate());
    if (!activity.isEmpty()) {
        provider.insert(QStringLiteral("activity"), activity);
    }
    provider.insert(QStringLiteral("sections"), sections);
    return {provider};
}

QVariantMap PopupModel::label() const {
    return m_fake
               ? m_label
               : claudecode::labelView(m_job->state(), QDateTime::currentDateTime(), MenuBarMode(m_menuBarMode),
                                       github::githubMenuBar(m_githubJob->state(), QDate::currentDate(),
                                                             QLocale().firstDayOfWeek(), QDateTime::currentDateTime()));
}
