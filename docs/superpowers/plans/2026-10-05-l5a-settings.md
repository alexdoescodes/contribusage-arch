# L-5a: Notifications and Settings Pages — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Limit notifications reach the desktop (FR-13 to FR-15, at most once per threshold and cycle, also across restarts), and the widget's settings pages set the label mode, style and tint, notification thresholds, the Claude Code path and probe interval, GitHub on/off and interval, and offer the data actions.

**Architecture:**
- **Core:** a `NotificationSink` seam (`core/seams.h`), `notificationstore` (the planner's cycles under `notifications` in `state.json`), `widgetsettings` (the plasmoid configuration as a validated `WidgetSettings`), `SourceSchedule::setPolicy`.
- **Jobs:** `LimitsJob` plans notifications after each success and hands them to its sink; `GitHubJob::setEnabled` (FR-43).
- **Platform:** `DesktopNotifier` sends to `org.freedesktop.Notifications` over D-Bus, replacing a cycle's earlier note (ADR-046).
- **Applet:** `PopupModel.applySettings(map)` applies the configuration; `main.qml` calls it with `Plasmoid.configuration` at start and on every change; config pages under `config/` (`config.qml`, `ConfigGeneral.qml`, `ConfigClaudeCode.qml`, `ConfigGitHub.qml`, `ConfigAdvanced.qml`).
- **Out of scope (L-5b):** the Popover tab and `PopoverLayout` (FR-44 to FR-47), Low Power Mode.

**Tech Stack:** C++23, Qt 6 Core/DBus, QML, KCMUtils (`SimpleKCM`), Kirigami FormLayout, Qt Test.

**Spec:** SPEC §11.6 (tabs), §10.7 (keys), FR-12, FR-13 to FR-15, FR-43, FR-51, US-12; ADR-027, ADR-028, ADR-038, ADR-043.

## Global Constraints

- C++23, `-Werror`, clang-format clean (only `*.cpp`/`*.h`), `QStringLiteral`, `QString(...)` around `QStringBuilder` results before `QVariant`; core links only Qt6::Core.
- Ranges (SPEC §11.6, FR-13): probe interval 1 to 60 min (default 5); GitHub interval 10 to 360 min (default 30); up to 3 thresholds between 50 and 99 (default 80, 95); notify on reset off by default; GitHub on by default.
- The token never enters the configuration (it goes through `PopupModel.connectGitHub` to KWallet only).
- "Reset caches" never touches `history.json` or the notification keys (ADR-028).

## Review Focus

1. **The widget restarts while a window is above 80 %.** Expected: no second notification. Pinned in Task 2 (`aRestartDoesNotNotifyAgain`).
2. **A configuration value out of range** (hand-edited file, interval 0, threshold 120). Expected: clamped or ignored, never a 0 ms timer. Pinned in Task 3 (`settingsAreClampedAndFiltered`).
3. **GitHub switched off.** Expected: no requests, block and layer gone, token kept. Pinned in Task 3 (`disabledGitHubSendsNothing`).
4. **No notification server running.** Expected: nothing breaks; the cycle still counts as sent. Pinned in Task 2 (the sink is fire-and-forget; `notesGoToTheSink`).
5. **Changing the probe interval.** Expected: the next run follows the new interval without a restart. Pinned in Task 3 (`aNewPolicyRearms`).

---

### Task 1: Notification seam and store

**Files:**
- Modify: `src/core/seams.h`, `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`
- Create: `src/core/notificationstore.h`, `src/core/notificationstore.cpp`, `tests/core/tst_notificationstore.cpp`, `tests/support/fakenotificationsink.h`

**Interfaces:**
- Produces:
  - In `core/seams.h`: `class NotificationSink { public: virtual ~NotificationSink() = default; virtual void notify(const NotificationNote &note) = 0; };` (includes `core/notificationplanner.h`)
  - `core/notificationstore.h`: `namespace notificationstore { NotificationsSent load(const AppPaths &); bool save(const AppPaths &, const NotificationsSent &); }`
  - Tests: `FakeNotificationSink` with `QList<NotificationNote> notes`.

- [ ] **Step 1: Write the failing test**

`tests/core/tst_notificationstore.cpp`:
```cpp
#include "core/limitscache.h"
#include "core/notificationstore.h"

#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

// FR-15: the planner's cycles survive a restart, in state.json beside the other caches.
class TestNotificationStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTripsBesideTheLimits() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        const QDateTime sentAt(QDate(2026, 10, 5), QTime(9, 0), QTimeZone::UTC);
        NotificationsSent sent;
        sent[QStringLiteral("claude-code")][QStringLiteral("Current session")] =
            NotificationCycle{.resetsAt = sentAt.addSecs(3600), .threshold = 80, .sentAt = sentAt};
        sent[QStringLiteral("claude-code")][QStringLiteral("Current week (all models)")] =
            NotificationCycle{.resetsAt = std::nullopt, .threshold = 95, .sentAt = sentAt};
        const Snapshot<LimitsReport> limits{
            .value = {.provider = QStringLiteral("claude-code"), .windows = {}, .billingNote = {}, .insights = {}, .rawOutput = {}},
            .fetchedAt = sentAt,
            .origin = origins::poll()};
        QVERIFY(limitscache::save(paths, limits));
        QVERIFY(notificationstore::save(paths, sent));
        QCOMPARE(notificationstore::load(paths), sent);
        QVERIFY(limitscache::load(paths, QStringLiteral("claude-code"))); // kept
    }

    void aMissingOrBrokenEntryIsEmpty() {
        QTemporaryDir dir;
        QVERIFY(notificationstore::load({.root = dir.path()}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestNotificationStore)
#include "tst_notificationstore.moc"
```

`tests/support/fakenotificationsink.h`:
```cpp
#pragma once

#include "core/seams.h"

// Records what would reach the desktop.
class FakeNotificationSink final : public contribusage::NotificationSink {
public:
    void notify(const contribusage::NotificationNote &note) override { notes.append(note); }

    QList<contribusage::NotificationNote> notes;
};
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_notificationstore.cpp
    TEST_NAME tst_notificationstore
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/notificationstore.h: No such file or directory`.

- [ ] **Step 3: Write the seam and the store**

In `src/core/seams.h`, add `#include "core/notificationplanner.h"` and, before the closing namespace:
```cpp
// Delivers a planned notification to the desktop (FR-13, FR-14). Fire and forget: a missing notification server must
// not change what the planner counts as sent.
class NotificationSink {
public:
    virtual ~NotificationSink() = default;
    virtual void notify(const NotificationNote &note) = 0;
};
```

`src/core/notificationstore.h`:
```cpp
#pragma once

#include "core/apppaths.h"
#include "core/notificationplanner.h"

namespace contribusage::notificationstore {

// FR-15: the planner's cycles under `notifications` in state.json; empty when missing or unreadable.
NotificationsSent load(const AppPaths &paths);
// Keeps the file's other entries.
bool save(const AppPaths &paths, const NotificationsSent &sent);

} // namespace contribusage::notificationstore
```

`src/core/notificationstore.cpp`:
```cpp
#include "core/notificationstore.h"

#include "core/jsonstore.h"
#include "core/limitscache.h"

#include <QJsonObject>

namespace contribusage::notificationstore {

namespace {

QString file(const AppPaths &paths) {
    return paths.root + QStringLiteral("/state.json");
}

QJsonObject state(const AppPaths &paths) {
    const auto read = jsonstore::read(file(paths), limitscache::schemaVersion);
    return read && read->has_value() ? read->value().toObject() : QJsonObject();
}

} // namespace

NotificationsSent load(const AppPaths &paths) {
    NotificationsSent sent;
    const QJsonObject providers = state(paths).value(QStringLiteral("notifications")).toObject();
    for (auto provider = providers.begin(); provider != providers.end(); ++provider) {
        const QJsonObject windows = provider.value().toObject();
        for (auto window = windows.begin(); window != windows.end(); ++window) {
            const QJsonObject cycle = window.value().toObject();
            const QDateTime sentAt =
                QDateTime::fromString(cycle.value(QStringLiteral("sentAt")).toString(), Qt::ISODateWithMs);
            const int threshold = cycle.value(QStringLiteral("threshold")).toInt();
            if (!sentAt.isValid() || threshold <= 0) {
                continue;
            }
            std::optional<QDateTime> resetsAt;
            if (const QString text = cycle.value(QStringLiteral("resetsAt")).toString(); !text.isEmpty()) {
                resetsAt = QDateTime::fromString(text, Qt::ISODateWithMs);
            }
            sent[provider.key()][window.key()] = {.resetsAt = resetsAt, .threshold = threshold, .sentAt = sentAt};
        }
    }
    return sent;
}

bool save(const AppPaths &paths, const NotificationsSent &sent) {
    QJsonObject providers;
    for (auto provider = sent.cbegin(); provider != sent.cend(); ++provider) {
        QJsonObject windows;
        for (auto window = provider->cbegin(); window != provider->cend(); ++window) {
            QJsonObject cycle{{QStringLiteral("threshold"), window->threshold},
                              {QStringLiteral("sentAt"), window->sentAt.toUTC().toString(Qt::ISODateWithMs)}};
            if (window->resetsAt) {
                cycle.insert(QStringLiteral("resetsAt"), window->resetsAt->toUTC().toString(Qt::ISODateWithMs));
            }
            windows.insert(window.key(), cycle);
        }
        providers.insert(provider.key(), windows);
    }
    QJsonObject root = state(paths);
    root.insert(QStringLiteral("notifications"), providers);
    return jsonstore::write(file(paths), limitscache::schemaVersion, root).has_value();
}

} // namespace contribusage::notificationstore
```

In `src/core/CMakeLists.txt`, add `notificationstore.cpp notificationstore.h` after `notificationplanner.h`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_notificationstore`
Expected: PASS. (If `QCOMPARE` on `NotificationsSent` fails only because a `QDateTime` lost its time spec, compare `toUTC()` values: the store writes UTC.)

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/notificationstore.* src/core/seams.h tests/core/tst_notificationstore.cpp tests/support/fakenotificationsink.h && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: notification sink seam and the planner's cycles in state.json

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Notifications from the limits job, and the desktop notifier

**Files:**
- Modify: `src/claudecode/limitsjob.h`, `src/claudecode/limitsjob.cpp`, `tests/claudecode/tst_limitsjob.cpp`, `src/platform/CMakeLists.txt`
- Create: `src/platform/desktopnotifier.h`, `src/platform/desktopnotifier.cpp`

**Interfaces:**
- Consumes: `NotificationSink`, `notificationstore`, `planNotifications`, `NotificationSettings`.
- Produces:
  - `LimitsJob::setNotifications(NotificationSink *sink, NotificationSettings settings)` (nullptr: no notifications).
  - `class DesktopNotifier final : public QObject, public NotificationSink` (`platform/desktopnotifier.h`): `org.freedesktop.Notifications.Notify` with app name `contribusage`, icon `utilities-system-monitor`, the note's title as summary, and `replaces_id` from the last server id of the same `note.id` (FR-52).

- [ ] **Step 1: Write the failing tests**

Add to `tests/claudecode/tst_limitsjob.cpp` (`#include "core/notificationstore.h"` and `#include "support/fakenotificationsink.h"`):
```cpp
    // FR-13: crossing a threshold notifies once; Review Focus 4: what the sink does is not the planner's business.
    void notesGoToTheSink() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        FakeNotificationSink sink;
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.setNotifications(&sink, {.thresholds = {50, 60}, .notifyOnReset = false});
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QVERIFY(!sink.notes.isEmpty());
        QVERIFY(sink.notes.first().title.startsWith(QStringLiteral("Claude Code: ")));
        const qsizetype sent = sink.notes.size();
        job.refresh();
        QTest::qWait(100);
        QCOMPARE(sink.notes.size(), sent);
        QVERIFY(!notificationstore::load(fixture.paths()).isEmpty());
    }

    // Review Focus 1: a restart does not notify the same cycle again.
    void aRestartDoesNotNotifyAgain() {
        Fixture fixture;
        fixture.runner.answer(claude, QStringLiteral("-p"), FakeProcessRunner::exited(0, ::fixture("subscription-basic")));
        FakeNotificationSink first;
        {
            LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
            job.setNotifications(&first, {.thresholds = {50, 60}, .notifyOnReset = false});
            job.start();
            QTRY_COMPARE(job.state().phase, Phase::Loaded);
        }
        QVERIFY(!first.notes.isEmpty());
        FakeNotificationSink second;
        LimitsJob job(fixture.limits, fixture.paths(), SchedulePolicy::every(60'000));
        job.setNotifications(&second, {.thresholds = {50, 60}, .notifyOnReset = false});
        job.start();
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
        QTest::qWait(50);
        QVERIFY(second.notes.isEmpty());
    }
```
(The `subscription-basic` fixture has windows above 50 %; if it does not, use thresholds below its lowest window and note that in the ledger.)

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake --build build`
Expected: FAIL to compile (`LimitsJob` has no member `setNotifications`).

- [ ] **Step 3: Plan and deliver**

`src/claudecode/limitsjob.h` — add `#include "core/notificationplanner.h"` and `#include "core/seams.h"`; add the public method:
```cpp
    // FR-13 to FR-15: after each success the planner's notes go to `sink` (nullptr: none); its cycles persist in
    // state.json so a restart does not repeat them.
    void setNotifications(NotificationSink *sink, NotificationSettings settings);
```
and the members:
```cpp
    NotificationSink *m_sink = nullptr;
    NotificationSettings m_notificationSettings;
    NotificationsSent m_sent;
```

`src/claudecode/limitsjob.cpp` — add `#include "core/notificationstore.h"`; in `start()`, before `m_schedule.start();`, add `m_sent = notificationstore::load(m_paths);`; add:
```cpp
void LimitsJob::setNotifications(NotificationSink *sink, NotificationSettings settings) {
    m_sink = sink;
    m_notificationSettings = std::move(settings);
}
```
and in `finish`, inside the success branch after `m_state = SourceState<LimitsReport>::loaded(snapshot);`, add:
```cpp
        if (m_sink) {
            const NotificationsSent before = m_sent;
            const QList<NotificationNote> notes =
                planNotifications(snapshot.value, QStringLiteral("Claude Code"), [](const QString &label) { return label; },
                                  m_notificationSettings, m_sent, now);
            for (const NotificationNote &note : notes) {
                m_sink->notify(note);
            }
            if (m_sent != before) {
                notificationstore::save(m_paths, m_sent);
            }
        }
```

`src/platform/desktopnotifier.h`:
```cpp
#pragma once

#include "core/seams.h"

#include <QHash>
#include <QObject>

namespace contribusage {

// ADR-046: notifications through the freedesktop notification service (Plasma's own), so nothing needs installing.
// A later note of the same window cycle replaces the earlier one (FR-52).
class DesktopNotifier final : public QObject, public NotificationSink {
    Q_OBJECT

public:
    using QObject::QObject;

    void notify(const NotificationNote &note) override;

private:
    QHash<QString, uint> m_serverIds; // note id → the server's id
};

} // namespace contribusage
```

`src/platform/desktopnotifier.cpp`:
```cpp
#include "platform/desktopnotifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>
#include <QVariantMap>

namespace contribusage {

void DesktopNotifier::notify(const NotificationNote &note) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    message << QStringLiteral("contribusage") << m_serverIds.value(note.id, 0u)
            << QStringLiteral("utilities-system-monitor") << note.title << QString() << QStringList()
            << QVariantMap{{QStringLiteral("x-kde-origin-name"), note.provider}} << -1;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    const QString id = note.id;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, id](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<uint> reply = *call;
        if (reply.isValid()) {
            m_serverIds.insert(id, reply.value());
        }
        call->deleteLater();
    });
}

} // namespace contribusage
```

In `src/platform/CMakeLists.txt`, add `desktopnotifier.cpp desktopnotifier.h` to the sources.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_limitsjob`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/limitsjob.* src/platform/desktopnotifier.* tests/claudecode/tst_limitsjob.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src tests
git commit -m "Limit notifications: planned after each probe, persisted, sent to the desktop

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Settings model, policies and the GitHub switch

**Files:**
- Create: `src/core/widgetsettings.h`, `src/core/widgetsettings.cpp`, `tests/core/tst_widgetsettings.cpp`
- Modify: `src/core/sourceschedule.h`, `src/core/sourceschedule.cpp`, `tests/core/tst_sourceschedule.cpp`, `src/github/githubjob.h`, `src/github/githubjob.cpp`, `src/github/githubview.cpp`, `tests/github/tst_githubjob.cpp`, `src/claudecode/limitsjob.h`, `src/claudecode/limitsjob.cpp`, `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `core/widgetsettings.h`: `struct WidgetSettings { MenuBarMode menuBarMode = MenuBarMode::Primary; int probeIntervalMinutes = 5; bool githubEnabled = true; int githubIntervalMinutes = 30; NotificationSettings notifications; QString claudePathOverride; }`, `WidgetSettings widgetSettingsFromMap(const QVariantMap &)`, `MenuBarMode menuBarModeFromName(const QString &)`
  - `SourceSchedule::setPolicy(SchedulePolicy)` (re-arms).
  - `LimitsJob::setInterval(int minutes)`, `GitHubJob::setInterval(int minutes)`, `GitHubJob::setEnabled(bool)` (off: no runs, state NotConfigured(ProviderDisabled); `githubView`/`githubLayer` give an empty view and no layer for it).

- [ ] **Step 1: Write the failing tests**

`tests/core/tst_widgetsettings.cpp`:
```cpp
#include "core/widgetsettings.h"

#include <QTest>

using namespace contribusage;

// SPEC §10.7, §11.6: the plasmoid configuration as validated settings.
class TestWidgetSettings : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void defaultsFollowSpec() {
        const WidgetSettings settings = widgetSettingsFromMap({});
        QCOMPARE(settings.menuBarMode, MenuBarMode::Primary);
        QCOMPARE(settings.probeIntervalMinutes, 5);
        QVERIFY(settings.githubEnabled);
        QCOMPARE(settings.githubIntervalMinutes, 30);
        QCOMPARE(settings.notifications.thresholds, (QList<int>{80, 95}));
        QVERIFY(!settings.notifications.notifyOnReset);
    }

    // Review Focus 2: out of range values are clamped or dropped.
    void settingsAreClampedAndFiltered() {
        const WidgetSettings settings = widgetSettingsFromMap({{QStringLiteral("probeIntervalMinutes"), 0},
                                                               {QStringLiteral("githubIntervalMinutes"), 9999},
                                                               {QStringLiteral("notificationThresholds"), QStringLiteral("120,70,45,90,60")},
                                                               {QStringLiteral("menuBarMode"), QStringLiteral("nonsense")},
                                                               {QStringLiteral("notifyOnReset"), true},
                                                               {QStringLiteral("githubEnabled"), false},
                                                               {QStringLiteral("claudePathOverride"), QStringLiteral(" /opt/claude ")}});
        QCOMPARE(settings.probeIntervalMinutes, 1);
        QCOMPARE(settings.githubIntervalMinutes, 360);
        QCOMPARE(settings.notifications.thresholds, (QList<int>{60, 70, 90}));
        QCOMPARE(settings.menuBarMode, MenuBarMode::Primary);
        QVERIFY(settings.notifications.notifyOnReset);
        QVERIFY(!settings.githubEnabled);
        QCOMPARE(settings.claudePathOverride, QStringLiteral("/opt/claude"));
    }

    void modeNames() {
        QCOMPARE(menuBarModeFromName(QStringLiteral("weekly")), MenuBarMode::Weekly);
        QCOMPARE(menuBarModeFromName(QStringLiteral("highest")), MenuBarMode::Highest);
        QCOMPARE(menuBarModeFromName(QStringLiteral("githubToday")), MenuBarMode::GithubToday);
        QCOMPARE(menuBarModeFromName(QStringLiteral("primaryAndGitHub")), MenuBarMode::PrimaryAndGitHub);
        QCOMPARE(menuBarModeFromName(QStringLiteral("iconOnly")), MenuBarMode::IconOnly);
    }
};

QTEST_GUILESS_MAIN(TestWidgetSettings)
#include "tst_widgetsettings.moc"
```

Add to `TestSourceSchedule` in `tests/core/tst_sourceschedule.cpp`:
```cpp
    // Review Focus 5: a new interval applies to the next run.
    void aNewPolicyRearms() {
        SourceSchedule schedule(SchedulePolicy::every(60'000));
        QSignalSpy due(&schedule, &SourceSchedule::due);
        schedule.start();
        QTRY_COMPARE(due.size(), 1);
        schedule.finished(true);
        schedule.setPolicy(SchedulePolicy::every(100));
        QTRY_COMPARE(due.size(), 2);
    }
```

Add to `TestGitHubJob` in `tests/github/tst_githubjob.cpp`:
```cpp
    // Review Focus 3, FR-43: off sends nothing, hides the block and keeps the token.
    void disabledGitHubSendsNothing() {
        Fixture fixture;
        fixture.secrets.values.insert(QStringLiteral("token"), QStringLiteral("ghp_x"));
        fixture.transport.respond(200, ::fixture("calendar"));
        GitHubJob job(fixture.account, fixture.paths(), SchedulePolicy::every(100));
        job.setEnabled(false);
        job.start();
        QTest::qWait(300);
        QVERIFY(fixture.transport.requests.isEmpty());
        QCOMPARE(job.state().phase, Phase::NotConfigured);
        QCOMPARE(job.state().reason, NotConfiguredReason::ProviderDisabled);
        QVERIFY(githubView(job.state(), QDateTime::currentDateTime()).isEmpty());
        QVERIFY(fixture.secrets.values.contains(QStringLiteral("token")));
        job.setEnabled(true);
        QTRY_COMPARE(job.state().phase, Phase::Loaded);
    }
```
(add `#include "github/githubview.h"` if the file lacks it, and `contribusage_github` already links it.)

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_widgetsettings.cpp
    TEST_NAME tst_widgetsettings
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL (`core/widgetsettings.h` missing; `setPolicy`, `setEnabled` unknown).

- [ ] **Step 3: Implement**

`src/core/widgetsettings.h`:
```cpp
#pragma once

#include "core/menubarlabel.h"
#include "core/notificationplanner.h"

#include <QString>
#include <QVariantMap>

namespace contribusage {

// SPEC §10.7's settings as the widget stores them in its plasmoid configuration (config/main.xml), validated.
struct WidgetSettings {
    MenuBarMode menuBarMode = MenuBarMode::Primary;
    int probeIntervalMinutes = 5; // 1 to 60 (SPEC §11.6)
    bool githubEnabled = true;    // FR-43
    int githubIntervalMinutes = 30; // 10 to 360
    NotificationSettings notifications;
    QString claudePathOverride;
};

// `menuBarMode` by its SPEC §10.7 name; unknown names give Primary.
MenuBarMode menuBarModeFromName(const QString &name);

// From Plasmoid.configuration's values; missing keys keep the defaults, out of range values are clamped, thresholds
// outside 50 to 99 are dropped and at most 3 kept (FR-13).
WidgetSettings widgetSettingsFromMap(const QVariantMap &map);

} // namespace contribusage
```

`src/core/widgetsettings.cpp`:
```cpp
#include "core/widgetsettings.h"

#include <QStringList>

#include <algorithm>

namespace contribusage {

MenuBarMode menuBarModeFromName(const QString &name) {
    if (name == QStringLiteral("weekly")) {
        return MenuBarMode::Weekly;
    }
    if (name == QStringLiteral("highest")) {
        return MenuBarMode::Highest;
    }
    if (name == QStringLiteral("githubToday")) {
        return MenuBarMode::GithubToday;
    }
    if (name == QStringLiteral("primaryAndGitHub")) {
        return MenuBarMode::PrimaryAndGitHub;
    }
    if (name == QStringLiteral("iconOnly")) {
        return MenuBarMode::IconOnly;
    }
    return MenuBarMode::Primary;
}

WidgetSettings widgetSettingsFromMap(const QVariantMap &map) {
    WidgetSettings settings;
    settings.menuBarMode = menuBarModeFromName(map.value(QStringLiteral("menuBarMode")).toString());
    if (map.contains(QStringLiteral("probeIntervalMinutes"))) {
        settings.probeIntervalMinutes = std::clamp(map.value(QStringLiteral("probeIntervalMinutes")).toInt(), 1, 60);
    }
    if (map.contains(QStringLiteral("githubIntervalMinutes"))) {
        settings.githubIntervalMinutes = std::clamp(map.value(QStringLiteral("githubIntervalMinutes")).toInt(), 10, 360);
    }
    if (map.contains(QStringLiteral("githubEnabled"))) {
        settings.githubEnabled = map.value(QStringLiteral("githubEnabled")).toBool();
    }
    if (map.contains(QStringLiteral("notifyOnReset"))) {
        settings.notifications.notifyOnReset = map.value(QStringLiteral("notifyOnReset")).toBool();
    }
    if (map.contains(QStringLiteral("notificationThresholds"))) {
        QList<int> thresholds;
        for (const QString &part :
             map.value(QStringLiteral("notificationThresholds")).toString().split(u',', Qt::SkipEmptyParts)) {
            bool ok = false;
            const int value = part.trimmed().toInt(&ok);
            if (ok && value >= 50 && value <= 99 && !thresholds.contains(value)) {
                thresholds.append(value);
            }
        }
        std::ranges::sort(thresholds);
        settings.notifications.thresholds = thresholds.mid(0, 3);
    }
    settings.claudePathOverride = map.value(QStringLiteral("claudePathOverride")).toString().trimmed();
    return settings;
}

} // namespace contribusage
```

`src/core/sourceschedule.h/.cpp` — add `void setPolicy(SchedulePolicy policy);` (public, documented "A new policy (a changed interval setting) applies from the next run") implemented as `m_policy = policy; arm();`.

`src/claudecode/limitsjob.h/.cpp` — add `void setInterval(int minutes);`:
```cpp
void LimitsJob::setInterval(int minutes) {
    SchedulePolicy policy = m_schedule.policy();
    policy.defaultMs = qint64(minutes) * 60'000;
    m_schedule.setPolicy(policy);
}
```

`src/github/githubjob.h/.cpp` — add `void setInterval(int minutes);` (same body) and:
```cpp
// FR-43: off makes no requests and shows as disabled; the token stays. On again fetches as after a token change.
void setEnabled(bool enabled);
```
with the member `bool m_enabled = true;` and:
```cpp
void GitHubJob::setEnabled(bool enabled) {
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    if (!enabled) {
        ++m_generation; // a running fetch finishes into nothing
        m_schedule.setPaused(true);
        m_state = SourceState<GitHubReport>::notConfigured(NotConfiguredReason::ProviderDisabled);
        Q_EMIT stateChanged();
        return;
    }
    m_state = SourceState<GitHubReport>::loading(githubcache::load(m_paths));
    Q_EMIT stateChanged();
    m_schedule.reset();
}
```
In `run()`, return at once when `!m_enabled` after calling `m_schedule.finished(false, false);`; in `refresh()` and `popupOpened()`, return when `!m_enabled`; in `start()`, when `!m_enabled` set the disabled state, emit, and do not start the schedule (start it in `setEnabled(true)` instead: replace `m_schedule.reset()` there by `m_schedule.reset(); m_schedule.start();`).

`src/github/githubview.cpp` — at the start of `githubView`, return `{}` when `state.phase == Phase::NotConfigured && state.reason == NotConfiguredReason::ProviderDisabled`; at the start of `githubLayer`, return `std::nullopt` in the same case.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i src/core/widgetsettings.* src/core/sourceschedule.* src/claudecode/limitsjob.* src/github/githubjob.* src/github/githubview.cpp tests/core/tst_widgetsettings.cpp tests/core/tst_sourceschedule.cpp tests/github/tst_githubjob.cpp
git add src tests
git commit -m "Settings model, live interval changes and the GitHub switch

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Settings pages and applying them

**Files:**
- Modify: `src/applet/config/main.xml`, `src/applet/CMakeLists.txt`, `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`, `src/applet/main.qml`, `tests/applet/tst_appletqml.cpp`
- Create: `src/applet/config/config.qml`, `src/applet/ConfigGeneral.qml`, `src/applet/ConfigClaudeCode.qml`, `src/applet/ConfigGitHub.qml`, `src/applet/ConfigAdvanced.qml`

**Interfaces:**
- Consumes: Tasks 1 to 3.
- Produces: `PopupModel` gains `Q_INVOKABLE void applySettings(const QVariantMap &configuration)`, `Q_INVOKABLE void deleteClaudeCodeData()`, `Q_INVOKABLE void resetCaches()`, `Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)`; `label` follows the configured mode.

- [ ] **Step 1: Write the failing tests**

Add `"ConfigGeneral"`, `"ConfigClaudeCode"`, `"ConfigGitHub"`, `"ConfigAdvanced"` to `qmlFileCompiles_data` in `tests/applet/tst_appletqml.cpp`, and add:
```cpp
    // SPEC §11.6: the configuration dialog's categories.
    void configModelListsTheTabs() {
        QFile file(QStringLiteral(":/qt/qml/plasma/applet/org/contribusage/widget/config/config.qml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        for (const char *page : {"ConfigGeneral.qml", "ConfigClaudeCode.qml", "ConfigGitHub.qml", "ConfigAdvanced.qml"}) {
            QVERIFY2(text.contains(page), page);
        }
    }

    // The schema holds every SPEC §10.7 key the pages edit.
    void schemaHoldsTheSettings() {
        QFile file(QStringLiteral(":/qt/qml/plasma/applet/org/contribusage/widget/config/main.xml"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        for (const char *key : {"menuBarMode", "menuBarStyle", "menuBarTint", "menuBarColor", "notificationThresholds",
                                "notifyOnReset", "probeIntervalMinutes", "claudePathOverride", "githubEnabled",
                                "githubIntervalMinutes"}) {
            QVERIFY2(text.contains(QByteArray("name=\"") + key + '"'), key);
        }
    }

    // FR-12: the fake model follows the configured mode too.
    void applySettingsIsCallable() {
        QQmlEngine engine;
        auto *model = engine.singletonInstance<QObject *>(QString::fromLatin1(uri), QStringLiteral("PopupModel"));
        QVERIFY(QMetaObject::invokeMethod(model, "applySettings",
                                          Q_ARG(QVariantMap, (QVariantMap{{QStringLiteral("menuBarMode"), QStringLiteral("weekly")}}))));
        QVERIFY(!model->property("dataFolder").toString().isEmpty());
    }
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R tst_appletqml`
Expected: FAIL (the config pages and keys do not exist; `applySettings` is unknown).

- [ ] **Step 3: Write the pages and apply them**

`src/applet/config/main.xml` — inside `<group name="General">`, after `menuBarColor`, add:
```xml
    <entry name="menuBarMode" type="String"><default>primary</default></entry>
    <entry name="notificationThresholds" type="String"><default>80,95</default></entry>
    <entry name="notifyOnReset" type="Bool"><default>false</default></entry>
    <entry name="probeIntervalMinutes" type="Int"><default>5</default></entry>
    <entry name="claudePathOverride" type="String"><default></default></entry>
    <entry name="githubEnabled" type="Bool"><default>true</default></entry>
    <entry name="githubIntervalMinutes" type="Int"><default>30</default></entry>
```

`src/applet/config/config.qml`:
```qml
import QtQuick
import org.kde.plasma.configuration

// SPEC §11.6's tabs that L-5a covers; the Popover tab follows with L-5b.
ConfigModel {
    ConfigCategory {
        name: "General"
        icon: "preferences-desktop"
        source: "ConfigGeneral.qml"
    }
    ConfigCategory {
        name: "Claude Code"
        icon: "draw-star"
        source: "ConfigClaudeCode.qml"
    }
    ConfigCategory {
        name: "GitHub"
        icon: "vcs-commit"
        source: "ConfigGitHub.qml"
    }
    ConfigCategory {
        name: "Advanced"
        icon: "configure"
        source: "ConfigAdvanced.qml"
    }
}
```

`src/applet/ConfigGeneral.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 General: label mode, style (with a live preview), color, notifications.
KCM.SimpleKCM {
    property string cfg_menuBarMode
    property string cfg_menuBarStyle
    property string cfg_menuBarTint
    property string cfg_menuBarColor
    property string cfg_notificationThresholds
    property bool cfg_notifyOnReset

    readonly property var modes: [["primary", "Session"], ["weekly", "Week"], ["highest", "Highest window"],
        ["githubToday", "GitHub today"], ["primaryAndGitHub", "Session and GitHub"], ["iconOnly", "Icon only"]]
    readonly property var styles: [["prompt", "Prompt"], ["rings", "Rings"], ["ring", "Ring"], ["line", "Line"],
        ["heatmap", "Heatmap"], ["sharedHeatmap", "Shared heatmap"], ["text", "Text"]]
    readonly property var tints: [["provider", "Source color"], ["usage", "By usage"], ["accent", "Accent color"],
        ["monochrome", "Monochrome"], ["custom", "Custom"]]

    function indexOf(list, key) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i][0] === key) return i;
        }
        return 0;
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            Kirigami.FormData.label: "Panel shows:"
            model: modes.map(m => m[1])
            currentIndex: indexOf(modes, cfg_menuBarMode)
            onActivated: index => cfg_menuBarMode = modes[index][0]
            Accessible.name: "Panel shows"
        }
        QQC2.ComboBox {
            id: styleBox
            Kirigami.FormData.label: "Style:"
            model: styles.map(s => s[1])
            currentIndex: indexOf(styles, cfg_menuBarStyle)
            onActivated: index => cfg_menuBarStyle = styles[index][0]
            Accessible.name: "Label style"
        }
        LabelContent {
            Kirigami.FormData.label: "Preview:"
            style: cfg_menuBarStyle
            tint: cfg_menuBarTint
            customColor: cfg_menuBarColor
            label: PopupModel.label
            heatmap: PopupModel.heatmap
        }
        QQC2.ComboBox {
            Kirigami.FormData.label: "Color:"
            model: tints.map(t => t[1])
            currentIndex: indexOf(tints, cfg_menuBarTint)
            onActivated: index => cfg_menuBarTint = tints[index][0]
            Accessible.name: "Label color"
        }
        QQC2.TextField {
            Kirigami.FormData.label: "Custom color:"
            visible: cfg_menuBarTint === "custom"
            text: cfg_menuBarColor
            placeholderText: "#ff9f0a"
            onEditingFinished: cfg_menuBarColor = text.trim()
            Accessible.name: "Custom color"
        }
        Item { Kirigami.FormData.isSection: true }
        QQC2.TextField {
            Kirigami.FormData.label: "Notify at (%):"
            text: cfg_notificationThresholds
            placeholderText: "80,95"
            onEditingFinished: cfg_notificationThresholds = text
            Accessible.name: "Notification thresholds, up to three values between 50 and 99"
        }
        QQC2.CheckBox {
            text: "Notify when a window resets"
            checked: cfg_notifyOnReset
            onToggled: cfg_notifyOnReset = checked
        }
    }
}
```

`src/applet/ConfigClaudeCode.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 Providers, for Claude Code: path override, probe interval, delete its data (US-12).
KCM.SimpleKCM {
    property string cfg_claudePathOverride
    property int cfg_probeIntervalMinutes

    Kirigami.FormLayout {
        QQC2.TextField {
            Kirigami.FormData.label: "Path to claude:"
            text: cfg_claudePathOverride
            placeholderText: "found automatically"
            onEditingFinished: cfg_claudePathOverride = text.trim()
            Accessible.name: "Path to claude"
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: "Check limits every (min):"
            from: 1
            to: 60
            value: cfg_probeIntervalMinutes
            onValueModified: cfg_probeIntervalMinutes = value
            Accessible.name: "Probe interval in minutes"
        }
        Item { Kirigami.FormData.isSection: true }
        QQC2.Button {
            text: "Delete data for Claude Code…"
            onClicked: confirm.open()
        }
        Kirigami.PromptDialog {
            id: confirm
            title: "Delete Claude Code data?"
            subtitle: "This removes the cached limits and the activity history kept by the widget. Claude Code's own files stay."
            standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
            onAccepted: PopupModel.deleteClaudeCodeData()
        }
    }
}
```

`src/applet/ConfigGitHub.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 GitHub: switch, account, token (validated, then KWallet), interval.
KCM.SimpleKCM {
    property bool cfg_githubEnabled
    property int cfg_githubIntervalMinutes

    Kirigami.FormLayout {
        QQC2.Switch {
            Kirigami.FormData.label: "GitHub:"
            text: checked ? "On" : "Off"
            checked: cfg_githubEnabled
            onToggled: cfg_githubEnabled = checked
            Accessible.name: "GitHub on or off"
        }
        QQC2.Label {
            Kirigami.FormData.label: "Account:"
            text: (PopupModel.github.login ?? "").length > 0 ? PopupModel.github.login : "Not connected"
        }
        RowLayout {
            Kirigami.FormData.label: (PopupModel.github.login ?? "").length > 0 ? "Replace token:" : "Token:"
            Kirigami.PasswordField {
                id: tokenField
                Layout.fillWidth: true
                Accessible.name: "GitHub token"
            }
            QQC2.Button {
                text: (PopupModel.github.login ?? "").length > 0 ? "Replace" : "Connect"
                enabled: tokenField.text.trim().length > 0
                onClicked: {
                    const token = tokenField.text.trim();
                    tokenField.text = "";
                    PopupModel.connectGitHub(token);
                }
            }
        }
        QQC2.Label {
            visible: (PopupModel.github.connectError ?? "").length > 0
            text: PopupModel.github.connectError ?? ""
            color: Kirigami.Theme.negativeTextColor
        }
        QQC2.Button {
            visible: (PopupModel.github.login ?? "").length > 0
            text: "Disconnect"
            onClicked: PopupModel.disconnectGitHub()
        }
        Kirigami.UrlButton {
            url: "https://github.com/settings/personal-access-tokens/new"
            text: "Create a read-only token"
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: "Refresh every (min):"
            from: 10
            to: 360
            value: cfg_githubIntervalMinutes
            onValueModified: cfg_githubIntervalMinutes = value
            Accessible.name: "GitHub refresh interval in minutes"
        }
    }
}
```

`src/applet/ConfigAdvanced.qml`:
```qml
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

// SPEC §11.6 Advanced: data folder, reset caches (never history or notification keys, ADR-028).
KCM.SimpleKCM {
    Kirigami.FormLayout {
        QQC2.Button {
            Kirigami.FormData.label: "Data:"
            text: "Open data folder"
            onClicked: Qt.openUrlExternally("file://" + PopupModel.dataFolder)
        }
        QQC2.Button {
            text: "Reset caches"
            onClicked: PopupModel.resetCaches()
        }
        QQC2.Label {
            text: "Resetting keeps the activity history and the record of sent notifications."
            wrapMode: Text.Wrap
            opacity: 0.7
        }
    }
}
```

`src/applet/CMakeLists.txt` — add `ConfigGeneral.qml ConfigClaudeCode.qml ConfigGitHub.qml ConfigAdvanced.qml` to `QML_SOURCES`, and add `config/config.qml` to the `ecm_target_qml_sources(... PATH config)` call (as a second source). If `config.qml` cannot go through `ecm_target_qml_sources` because it is a `.qml` file with a `ConfigModel` that the QML module would try to register as a type, add it with `qt_add_resources(org.contribusage.widget "applet_config" PREFIX "/qt/qml/plasma/applet/org/contribusage/widget" FILES config/config.qml)` instead.

`src/applet/popupmodel.h`:
- add `Q_PROPERTY(QString dataFolder READ dataFolder CONSTANT)`, `QString dataFolder() const;`
- add `Q_INVOKABLE void applySettings(const QVariantMap &configuration);`, `Q_INVOKABLE void deleteClaudeCodeData();`, `Q_INVOKABLE void resetCaches();`
- forward-declare `class DesktopNotifier;` in namespace `contribusage`; add members `std::unique_ptr<contribusage::DesktopNotifier> m_notifier;` and `int m_menuBarMode = 0; // MenuBarMode`

`src/applet/popupmodel.cpp`:
- add includes `core/widgetsettings.h`, `core/jsonstore.h`, `platform/desktopnotifier.h`
- in the live constructor branch after the limits job is made: `m_notifier = std::make_unique<DesktopNotifier>(); m_job->setNotifications(m_notifier.get(), {});`
- add:
```cpp
QString PopupModel::dataFolder() const {
    return AppPaths::live().root;
}

void PopupModel::applySettings(const QVariantMap &configuration) {
    const WidgetSettings settings = widgetSettingsFromMap(configuration);
    m_menuBarMode = int(settings.menuBarMode);
    if (!m_fake) {
        m_job->setInterval(settings.probeIntervalMinutes);
        m_job->setNotifications(m_notifier.get(), settings.notifications);
        m_limits->setOverride(settings.claudePathOverride);
        m_githubJob->setInterval(settings.githubIntervalMinutes);
        m_githubJob->setEnabled(settings.githubEnabled);
    }
    Q_EMIT changed();
}

void PopupModel::deleteClaudeCodeData() {
    if (!m_fake) {
        jsonstore::deleteProviderData(QStringLiteral("claude-code"), AppPaths::live()); // US-12
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
```
(add `#include "core/notificationstore.h"` and `#include <QFile>`)
- in `label()`, live: pass the mode to the label: replace the call with `claudecode::labelView(m_job->state(), QDateTime::currentDateTime(), MenuBarMode(m_menuBarMode))` and give `labelView` in `src/claudecode/limitsview.h/.cpp` a third parameter `MenuBarMode mode = MenuBarMode::Primary` passed to `menuBarLabel` (GitHub data for the GitHub modes arrives with the label source work in L-5b; until then they show `?`).

`src/applet/main.qml` — inside `PlasmoidItem`, add:
```qml
    // SPEC §10.7: the settings live in the plasmoid configuration; the model applies them at start and on change.
    function applySettings() {
        const c = Plasmoid.configuration;
        PopupModel.applySettings({
            "menuBarMode": c.menuBarMode,
            "notificationThresholds": c.notificationThresholds,
            "notifyOnReset": c.notifyOnReset,
            "probeIntervalMinutes": c.probeIntervalMinutes,
            "claudePathOverride": c.claudePathOverride,
            "githubEnabled": c.githubEnabled,
            "githubIntervalMinutes": c.githubIntervalMinutes
        });
    }
    Component.onCompleted: applySettings()
    Connections {
        target: Plasmoid.configuration
        function onValueChanged() { root.applySettings(); }
    }
```
(and `import org.kde.plasma.plasmoid` is already there.)

- [ ] **Step 4: Run every test and look at the dialog**

Run: `cmake -B build -G Ninja && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

Render `ConfigGeneral.qml` with the scratch renderer of L-4 (load the plugin, create the page with `cfg_menuBarStyle: "rings"` and the other `cfg_` values set) and take a screenshot. Expected: the form with the preview drawing the rings style. Open the viewer's configure dialog is not automatable here; note it in the ledger.

- [ ] **Step 5: Commit**

```bash
clang-format -i src/applet/*.cpp src/applet/*.h src/claudecode/limitsview.* tests/applet/tst_appletqml.cpp
git add src tests
git commit -m "Applet: settings pages, settings applied live, desktop notifications

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Contract and vault

**Files:**
- Create: `llm-wiki/decisions/0046-notifications-and-settings.md`
- Modify: `SPEC.md` (§17.8: split L-5 into L-5a done and L-5b open), `llm-wiki/modules/applet.md`, `llm-wiki/modules/core.md`, `llm-wiki/index.md`, `llm-wiki/log.md`

- [ ] **Step 1: Contract**

In `SPEC.md` §17.8, replace the `L-5` line with:
```markdown
- [x] **L-5a** Notifications and settings: the planner's notes delivered over `org.freedesktop.Notifications` with their cycles in `state.json` (FR-13 to FR-15, ADR-046); settings pages General, Claude Code, GitHub and Advanced (§11.6) in the plasmoid configuration, applied live (label mode and style, thresholds, notify on reset, probe and GitHub intervals, path override, GitHub on/off FR-43, delete data US-12, reset caches ADR-028). Accept: `tst_notificationstore`, `tst_widgetsettings`, the job and applet tests pass.
- [ ] **L-5b** The Popover tab and `PopoverLayout` (FR-44 to FR-47, work follows use), the GitHub label modes' data, Low Power Mode from power-profiles-daemon.
```

- [ ] **Step 2: ADR-046**

`llm-wiki/decisions/0046-notifications-and-settings.md` (today's date for `YYYY-MM-DD`):
```markdown
---
type: decision
status: accepted
updated: YYYY-MM-DD
aliases: [ADR-046]
tracks: [src/platform/desktopnotifier.cpp, src/core/widgetsettings.cpp, src/applet/main.qml]
tags: [notifications, settings, platform]
---
# ADR-046: Notifications over the freedesktop service; settings in the plasmoid configuration

- **Decided:** YYYY-MM-DD · **Spec:** FR-13 to FR-15, FR-52, SPEC §10.7, §11.6 · **Supersedes:** —

## Context
The macOS app delivered notifications through UserNotifications and kept settings in UserDefaults. A Plasma widget has KNotification and KConfig-backed plasmoid configuration; KNotification needs a `.notifyrc` installed with the package, which does not exist before L-7.

## Options
| Option | For | Against |
|---|---|---|
| KNotification | KDE's own API, per-event settings in System Settings | Needs an installed notifyrc; silent without it |
| `org.freedesktop.Notifications` over D-Bus | Works from the build tree and in any session; Plasma's server shows it | No per-event configuration in System Settings |
| Settings in a file of our own | Full control | Duplicates what Plasma already stores per widget |

## Decision
The platform's `DesktopNotifier` calls `Notify` asynchronously and replaces a window cycle's earlier note by its server id (FR-52). Settings are entries of the widget's `config/main.xml`; `main.qml` hands `Plasmoid.configuration` to `PopupModel.applySettings`, and `widgetSettingsFromMap` validates them in the core.

## Consequences
- Moving to KNotification later only swaps the sink once the package installs a notifyrc (L-7).
- Each widget instance has its own settings, as Plasma does for every widget.
```

- [ ] **Step 3: Vault**

- `llm-wiki/modules/core.md`: `updated:` today; add `notificationstore.h`, `widgetsettings.h`, `NotificationSink` in `seams.h`, `SourceSchedule::setPolicy`.
- `llm-wiki/modules/applet.md`: `updated:` today; add the config pages (`config/config.qml`, `ConfigGeneral/ClaudeCode/GitHub/Advanced.qml`), the schema keys, `applySettings`, `deleteClaudeCodeData`, `resetCaches`, `dataFolder`, and the `DesktopNotifier` wiring.
- `llm-wiki/index.md`: under Decisions, after ADR-045, add `- [[decisions/0046-notifications-and-settings]]: ADR-046, notifications over org.freedesktop.Notifications; settings in the plasmoid configuration`.
- `llm-wiki/log.md`, append:
```markdown

## [YYYY-MM-DD] feat | Notifications and settings (L-5a)
- Limit notifications reach the desktop once per threshold and cycle, also across restarts (ADR-046).
- Settings pages for the label, notifications, Claude Code, GitHub and data, applied live from the plasmoid configuration.
```

- [ ] **Step 4: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md llm-wiki
git commit -m "Spec: L-5a done; ADR-046 notifications and settings

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
