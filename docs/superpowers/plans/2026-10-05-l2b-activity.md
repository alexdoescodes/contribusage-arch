# L-1b + L-2b: Claude Code Activity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show Claude Code activity from the local transcripts: today's requests, sessions and tokens, the last 7 days, and the Claude Code layer of the heatmap, with history that survives Claude Code deleting old transcripts.

**Architecture:**
- **Core (`src/core/`, Qt6::Core only):** `IncrementalJsonlReader` (offset, identity, partial line per file), `HistoryStore` (frozen days in `providers/<id>/history.json`), `DirectoryWatcher` (inotify through `QFileSystemWatcher`, recursive, debounced) and a layered `heatmapView`. These are the L-1b parts; L-3 already delivered the secret store.
- **Claude Code (`src/claudecode/`):** `transcriptline` (decode one line), `transcriptindex` (roots, file list, live index, per-day aggregation with de-duplication), `activityjob` (reads on a worker `QThread`, so the plasmashell GUI thread never parses the ~0.5 GB of transcripts) and `activityview` (the activity block and the heatmap layer for QML).
- **Applet:** `PopupModel` adds `activity` to the Claude Code provider and draws the Claude Code and GitHub layers in one heatmap.

**Tech Stack:** C++23, Qt 6 Core (QFile, QFileSystemWatcher, QThread, QJsonDocument), QML, Qt Test.

**Spec:**
- **Requirements:** SPEC §8.3 (location, fields, aggregation, history, incremental reading), §10.4, §10.7, FR-22 to FR-29, FR-48 (heatmap quartiles), §11.2 (activity block), §13 (unreadable files retried).
- **Decisions:** ADR-024 (live index in memory), ADR-026 (rescan all files per change).
- **Port design:** `docs/superpowers/specs/2026-10-04-linux-plasmoid-port-design.md` §1 (FileEvents: QFileSystemWatcher, debounced; FileReading: QFile), transcripts paragraph.
- **Swift reference** (`git show 2603a21:<path>`): `Packages/ContribusageKit/Sources/ContribusageCore/Activity/{IncrementalJSONLReader,HistoryStore}.swift`, `Packages/ContribusageKit/Sources/ContribusageClaudeCode/Activity/{TranscriptLine,TranscriptAggregator,TranscriptFiles,TranscriptActivity}.swift` and their tests.

## Global Constraints

- C++23, `-Werror`, clang-format clean (only `*.cpp`/`*.h`), `QStringLiteral` everywhere (`QT_NO_CAST_FROM_ASCII`), wrap `QStringBuilder` concatenations in `QString(...)` before `QVariant`, no exceptions, `src/core/` links only Qt6::Core and includes nothing from providers.
- Nothing blocks the plasmashell GUI thread: transcript reading and history writes run on the worker thread.
- Claude Code's files are only read, never written (SPEC §2). The probe's own project folder never counts (FR-28).
- History: a day freezes once it lies before the day of `now - 48 h`; frozen days are never recomputed; keep 365 days; `history.json` is never discarded automatically (SPEC §8.3.4, §10.7).
- Day = local calendar day of `timestamp` in the system time zone (SPEC §8.3.3).

## Review Focus

1. **A transcript being written while it is read** (a partial last line). Expected: the partial line is held until its newline and counted once. Pinned in Task 1 (`readsOnlyAppendedCompleteLines`).
2. **Claude Code deletes old transcripts.** Expected: frozen days stay. Pinned in Task 2 (`frozenDaysAreNeverRecomputed`) and Task 6 (`frozenDaysSurviveDeletedTranscripts`).
3. **The same response in several files** (resumed sessions). Expected: counted once, the first file in path order wins. Pinned in Task 5 (`firstDuplicateInPathOrderCounts`).
4. **A damaged `history.json`.** Expected: left untouched on disk, the widget shows live days. Pinned in Task 2 (`unreadableHistoryIsNeverDiscarded`) and Task 6 (`damagedHistoryShowsLiveDays`).
5. **The widget is removed during a scan.** Expected: no crash, no hang longer than one file read. Pinned in Task 6 (`destroyedDuringAScan`).

---

## File Structure

| Path | Responsibility |
|---|---|
| `src/core/jsonlreader.h/.cpp` | `IncrementalJsonlReader` |
| `src/core/historystore.h/.cpp` | `HistoryStore` |
| `src/core/directorywatcher.h/.cpp` | `DirectoryWatcher` |
| `src/core/heatmap.h/.cpp` | adds `HeatmapLayer`, `heatmapView` |
| `src/github/githubview.h/.cpp` | adds `githubLayer`; `githubHeatmap` uses `heatmapView` |
| `src/claudecode/transcriptline.h/.cpp` | `TranscriptLine`, `decodeTranscriptLine` |
| `src/claudecode/transcriptindex.h/.cpp` | `transcriptRoots`, `transcriptFiles`, `TranscriptIndex` |
| `src/claudecode/activityjob.h/.cpp` | `ActivityWorker`, `ActivityJob` |
| `src/claudecode/activityview.h/.cpp` | `activityView`, `activityLayer` |
| `src/applet/popupmodel.h/.cpp` | live activity and the combined heatmap |
| `tests/core/tst_jsonlreader.cpp`, `tst_historystore.cpp`, `tst_directorywatcher.cpp` | core tests |
| `tests/claudecode/tst_transcriptline.cpp`, `tst_transcriptindex.cpp`, `tst_activityjob.cpp`, `tst_activityview.cpp` | provider tests |

---

### Task 1: Incremental JSONL reader

**Files:**
- Create: `src/core/jsonlreader.h`, `src/core/jsonlreader.cpp`, `tests/core/tst_jsonlreader.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (namespace `contribusage`, header `core/jsonlreader.h`):
  - `class IncrementalJsonlReader { enum class Outcome { Gone, Rescanned, Appended }; std::expected<Outcome, QString> read(const QString &path, const std::function<void(QByteArrayView)> &line); }`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_jsonlreader.cpp`:
```cpp
#include "core/jsonlreader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using Outcome = IncrementalJsonlReader::Outcome;

namespace {

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QFile file(path);
    QVERIFY(file.open(mode));
    QCOMPARE(file.write(bytes), bytes.size());
}

struct Read {
    std::expected<Outcome, QString> outcome;
    QList<QByteArray> lines;
};

Read read(IncrementalJsonlReader &reader, const QString &path) {
    Read result{.outcome = Outcome::Gone, .lines = {}};
    result.outcome = reader.read(path, [&result](QByteArrayView line) { result.lines.append(line.toByteArray()); });
    return result;
}

} // namespace

// SPEC §8.3.5, ported from IncrementalJSONLReaderTests.swift.
class TestJsonlReader : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // Review Focus 1: a partial last line waits for its newline and is handed over once.
    void readsOnlyAppendedCompleteLines() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\ntwo\nthr");
        IncrementalJsonlReader reader;
        const Read first = read(reader, path);
        QCOMPARE(first.outcome, Outcome::Appended);
        QCOMPARE(first.lines, (QList<QByteArray>{"one", "two"}));
        write(path, "ee\nfour\n", QIODevice::Append);
        const Read second = read(reader, path);
        QCOMPARE(second.outcome, Outcome::Appended);
        QCOMPARE(second.lines, (QList<QByteArray>{"three", "four"}));
        QCOMPARE(read(reader, path).lines, QList<QByteArray>{});
    }

    void emptyLinesAreLeftOut() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "a\n\n\nb\n");
        IncrementalJsonlReader reader;
        QCOMPARE(read(reader, path).lines, (QList<QByteArray>{"a", "b"}));
    }

    void rescansAShrunkFile() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\ntwo\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        write(path, "new\n"); // truncates
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Rescanned);
        QCOMPARE(again.lines, QList<QByteArray>{"new"});
    }

    void rescansAReplacedFile() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        const QString other = dir.filePath(QStringLiteral("b.jsonl"));
        write(other, "one\ntwo\nthree\n");
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(other, path));
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Rescanned);
        QCOMPARE(again.lines, (QList<QByteArray>{"one", "two", "three"}));
    }

    void deletedFileReadsGone() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        write(path, "one\n");
        IncrementalJsonlReader reader;
        read(reader, path);
        QVERIFY(QFile::remove(path));
        QCOMPARE(read(reader, path).outcome, Outcome::Gone);
        write(path, "one\n");
        const Read again = read(reader, path);
        QCOMPARE(again.outcome, Outcome::Appended); // a new file: nothing to drop
        QCOMPARE(again.lines, QList<QByteArray>{"one"});
    }

    void readsLinesAcrossBufferBoundaries() {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.jsonl"));
        const QByteArray longLine(200'000, 'x');
        write(path, longLine + "\nshort\n");
        IncrementalJsonlReader reader;
        const Read result = read(reader, path);
        QCOMPARE(result.lines.size(), qsizetype(2));
        QCOMPARE(result.lines.at(0), longLine);
        QCOMPARE(result.lines.at(1), QByteArray("short"));
    }
};

QTEST_GUILESS_MAIN(TestJsonlReader)
#include "tst_jsonlreader.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_jsonlreader.cpp
    TEST_NAME tst_jsonlreader
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/jsonlreader.h: No such file or directory`.

- [ ] **Step 3: Write the reader**

`src/core/jsonlreader.h`:
```cpp
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QHash>
#include <QString>

#include <expected>
#include <functional>

namespace contribusage {

// SPEC §8.3.5, FR-26: reads line files incrementally. Per file it keeps the identity (device and inode), the offset and
// a pending partial line, so each read hands over only the complete lines appended since the last one. Decoding the
// lines is the provider's job.
class IncrementalJsonlReader {
public:
    enum class Outcome {
        Gone,      // the file no longer exists; its state is dropped
        Rescanned, // read from 0 because its identity changed or it shrank: drop what it contributed before
        Appended,  // read on from the last offset: the lines add to what it contributed before
    };

    // Calls `line` with each complete line appended since the last read, in order, without its newline; empty lines are
    // left out. The file is streamed through a fixed buffer (NFR-2). On an error nothing is recorded: discard the lines
    // `line` received, the next read hands them over again.
    std::expected<Outcome, QString> read(const QString &path, const std::function<void(QByteArrayView)> &line);

private:
    struct State {
        quint64 device = 0;
        quint64 inode = 0;
        qint64 offset = 0;  // bytes read so far, `pending` included
        QByteArray pending; // grows with one line's length until its newline
    };
    QHash<QString, State> m_files;
};

} // namespace contribusage
```

`src/core/jsonlreader.cpp`:
```cpp
#include "core/jsonlreader.h"

#include <QFile>

#include <sys/stat.h>

namespace contribusage {

std::expected<IncrementalJsonlReader::Outcome, QString>
IncrementalJsonlReader::read(const QString &path, const std::function<void(QByteArrayView)> &line) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (!QFile::exists(path)) {
            m_files.remove(path);
            return Outcome::Gone;
        }
        return std::unexpected(file.errorString());
    }
    // Identity and size from the open descriptor, so a replace between stat and read cannot mix two files.
    struct stat info {};
    if (::fstat(file.handle(), &info) != 0) {
        return std::unexpected(QStringLiteral("cannot stat ") + path);
    }
    const auto previous = m_files.constFind(path);
    const bool known = previous != m_files.cend();
    const bool rescan = known && (previous->device != quint64(info.st_dev) || previous->inode != quint64(info.st_ino) ||
                                  qint64(info.st_size) < previous->offset);
    State state = known && !rescan ? *previous
                                   : State{.device = quint64(info.st_dev),
                                           .inode = quint64(info.st_ino),
                                           .offset = 0,
                                           .pending = {}};
    if (!file.seek(state.offset)) {
        return std::unexpected(file.errorString());
    }
    QByteArray buffer(1 << 16, Qt::Uninitialized);
    while (true) {
        const qint64 count = file.read(buffer.data(), buffer.size());
        if (count < 0) {
            return std::unexpected(file.errorString());
        }
        if (count == 0) {
            break;
        }
        state.offset += count;
        const QByteArrayView chunk(buffer.constData(), qsizetype(count));
        qsizetype start = 0;
        for (qsizetype newline = chunk.indexOf('\n', start); newline >= 0; newline = chunk.indexOf('\n', start)) {
            if (state.pending.isEmpty()) {
                if (newline > start) {
                    line(chunk.sliced(start, newline - start));
                }
            } else {
                state.pending.append(chunk.sliced(start, newline - start));
                line(state.pending);
                state.pending.clear();
            }
            start = newline + 1;
        }
        state.pending.append(chunk.sliced(start));
    }
    m_files.insert(path, state);
    return rescan ? Outcome::Rescanned : Outcome::Appended;
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `jsonlreader.cpp jsonlreader.h` to the sources (after `jsonstore.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_jsonlreader`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/jsonlreader.* tests/core/tst_jsonlreader.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: incremental JSONL reader

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: History store

**Files:**
- Create: `src/core/historystore.h`, `src/core/historystore.cpp`, `tests/core/tst_historystore.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `jsonstore::read/write`, `AppPaths::providerFolder`, `ActivityReport` and its JSON (`core/reports.h`).
- Produces (header `core/historystore.h`):
  - `class HistoryStore { static std::expected<HistoryStore, StoreError> open(const QString &providerId, const AppPaths &paths); QList<ActivityDay> merge(const QList<ActivityDay> &live, const QDateTime &now); const QList<ActivityDay> &frozen() const; }`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_historystore.cpp`:
```cpp
#include "core/historystore.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

namespace {

ActivityDay day(const char *iso, qint64 requests) {
    return {.day = QDate::fromString(QString::fromLatin1(iso), Qt::ISODate),
            .requests = requests,
            .sessions = 1,
            .tokens = {.input = requests, .output = 0, .cacheWrite = 0, .cacheRead = 0},
            .byModel = {}};
}

QList<QDate> dates(const QList<ActivityDay> &days) {
    QList<QDate> list;
    for (const ActivityDay &item : days) {
        list.append(item.day);
    }
    return list;
}

const QDateTime now(QDate(2026, 10, 5), QTime(12, 0));

} // namespace

// SPEC §8.3.4, ported from HistoryStoreTests.swift.
class TestHistoryStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // now - 48 h is 10-03 12:00, so 10-01 and 10-02 freeze; 10-03 onwards stays live.
    void freezesDaysMoreThan48HoursPast() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto store = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(store);
        const QList<ActivityDay> live{day("2026-10-01", 1), day("2026-10-02", 2), day("2026-10-03", 3),
                                      day("2026-10-04", 4), day("2026-10-05", 5)};
        QCOMPARE(store->merge(live, now), live);
        QCOMPARE(dates(store->frozen()), (QList<QDate>{QDate(2026, 10, 1), QDate(2026, 10, 2)}));
        const auto reopened = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(reopened);
        QCOMPARE(reopened->frozen(), store->frozen());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("providers/claude-code/history.json"))));
    }

    // Review Focus 2: a frozen day wins over its live count and stays when its transcripts are gone.
    void frozenDaysAreNeverRecomputed() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto store = HistoryStore::open(QStringLiteral("claude-code"), paths);
        QVERIFY(store);
        store->merge({day("2026-10-01", 5)}, now);
        QCOMPARE(store->merge({day("2026-10-01", 9)}, now), QList<ActivityDay>{day("2026-10-01", 5)});
        QCOMPARE(store->merge({day("2026-10-05", 1)}, now),
                 (QList<ActivityDay>{day("2026-10-01", 5), day("2026-10-05", 1)}));
    }

    // A frozen day is shown once, also when a clock change moves it back past the cut-off.
    void aFrozenDayShowsOnceWhenTheCutoffMovesBack() {
        QTemporaryDir dir;
        auto store = HistoryStore::open(QStringLiteral("claude-code"), {.root = dir.path()});
        QVERIFY(store);
        store->merge({day("2026-10-02", 2)}, now);
        const QList<ActivityDay> shown = store->merge({day("2026-10-02", 7)}, now.addDays(-3));
        QCOMPARE(shown, QList<ActivityDay>{day("2026-10-02", 2)});
    }

    // 365 days: 2025-10-06 is 364 days before 2026-10-05 and stays; the day before goes.
    void keeps365Days() {
        QTemporaryDir dir;
        auto store = HistoryStore::open(QStringLiteral("claude-code"), {.root = dir.path()});
        QVERIFY(store);
        store->merge({day("2025-10-05", 1), day("2025-10-06", 2)}, now);
        QCOMPARE(dates(store->frozen()), QList<QDate>{QDate(2025, 10, 6)});
    }

    void historiesAreKeptPerProvider() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        auto first = HistoryStore::open(QStringLiteral("claude-code"), paths);
        auto second = HistoryStore::open(QStringLiteral("other"), paths);
        QVERIFY(first && second);
        first->merge({day("2026-10-01", 1)}, now);
        QVERIFY(second->frozen().isEmpty());
        QVERIFY(HistoryStore::open(QStringLiteral("other"), paths)->frozen().isEmpty());
    }

    // Review Focus 4: an unreadable file is an error and stays as it is.
    void unreadableHistoryIsNeverDiscarded() {
        QTemporaryDir dir;
        const AppPaths paths{.root = dir.path()};
        QVERIFY(QDir().mkpath(dir.filePath(QStringLiteral("providers/claude-code"))));
        const QString file = dir.filePath(QStringLiteral("providers/claude-code/history.json"));
        {
            QFile out(file);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write("not json");
        }
        QVERIFY(!HistoryStore::open(QStringLiteral("claude-code"), paths));
        QFile in(file);
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), QByteArray("not json"));
    }
};

QTEST_GUILESS_MAIN(TestHistoryStore)
#include "tst_historystore.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_historystore.cpp
    TEST_NAME tst_historystore
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/historystore.h: No such file or directory`.

- [ ] **Step 3: Write the store**

`src/core/historystore.h`:
```cpp
#pragma once

#include "core/apppaths.h"
#include "core/jsonstore.h"
#include "core/reports.h"

#include <QDateTime>
#include <QList>
#include <QString>

#include <expected>

namespace contribusage {

// A provider's frozen daily aggregates in `providers/<id>/history.json` (SPEC §8.3.4, §10.7, FR-29). A day freezes once
// it lies before the day of `now - 48 h` and is never recomputed after that, so deleted transcripts keep their history.
class HistoryStore {
public:
    // An unreadable or other-version file is an error and stays on disk: history is never discarded.
    static std::expected<HistoryStore, StoreError> open(const QString &providerId, const AppPaths &paths);

    // Freezes the live days that are due, drops days beyond 365 and returns what to show, oldest first: frozen days,
    // then the live days not frozen. `live` holds the provider's current aggregates by local day, oldest first. A failed
    // write leaves the frozen days as they were, so the next merge tries again.
    QList<ActivityDay> merge(const QList<ActivityDay> &live, const QDateTime &now);

    const QList<ActivityDay> &frozen() const { return m_days; }

private:
    HistoryStore(QString providerId, QString file, QList<ActivityDay> days);

    QString m_providerId;
    QString m_file;
    QList<ActivityDay> m_days; // oldest first
};

} // namespace contribusage
```

`src/core/historystore.cpp`:
```cpp
#include "core/historystore.h"

#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace contribusage {

namespace {
constexpr int schemaVersion = 1;
}

HistoryStore::HistoryStore(QString providerId, QString file, QList<ActivityDay> days)
    : m_providerId(std::move(providerId)), m_file(std::move(file)), m_days(std::move(days)) {}

std::expected<HistoryStore, StoreError> HistoryStore::open(const QString &providerId, const AppPaths &paths) {
    const QString file = paths.providerFolder(providerId) + QStringLiteral("/history.json");
    const auto read = jsonstore::read(file, schemaVersion);
    if (!read) {
        return std::unexpected(read.error());
    }
    if (!read->has_value()) {
        return HistoryStore(providerId, file, {});
    }
    const auto report = activityReportFromJson(read->value().toObject());
    if (!report) {
        return std::unexpected(StoreError{.kind = StoreError::Kind::Malformed, .version = 0, .message = file});
    }
    return HistoryStore(providerId, file, report->days);
}

QList<ActivityDay> HistoryStore::merge(const QList<ActivityDay> &live, const QDateTime &now) {
    const QDate firstLive = now.addSecs(-48 * 3600).date();
    const QDate oldestKept = now.date().addDays(-364);
    QSet<QDate> frozen;
    for (const ActivityDay &day : m_days) {
        frozen.insert(day.day);
    }
    QList<ActivityDay> unfrozen;
    for (const ActivityDay &day : live) {
        if (!frozen.contains(day.day)) {
            unfrozen.append(day);
        }
    }
    QList<ActivityDay> updated;
    for (const ActivityDay &day : m_days) {
        if (day.day >= oldestKept) {
            updated.append(day);
        }
    }
    for (const ActivityDay &day : unfrozen) {
        if (day.day < firstLive && day.day >= oldestKept) {
            updated.append(day);
        }
    }
    std::ranges::sort(updated, {}, &ActivityDay::day);
    if (updated != m_days) {
        const ActivityReport report{.provider = m_providerId, .days = updated, .skippedLines = 0};
        if (jsonstore::write(m_file, schemaVersion, toJson(report))) {
            m_days = updated;
        }
    }
    QList<ActivityDay> shown = updated;
    for (const ActivityDay &day : unfrozen) {
        if (day.day >= firstLive) {
            shown.append(day);
        }
    }
    return shown;
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `historystore.cpp historystore.h` to the sources (after `heatmap.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_historystore`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/historystore.* tests/core/tst_historystore.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: history store with frozen days

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Directory watcher

**Files:**
- Create: `src/core/directorywatcher.h`, `src/core/directorywatcher.cpp`, `tests/core/tst_directorywatcher.cpp`
- Modify: `src/core/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Produces (header `core/directorywatcher.h`):
  - `class DirectoryWatcher : public QObject { DirectoryWatcher(QStringList roots, QString suffix, int debounceMs, QObject *parent = nullptr); void start(); signal changed(); }`

- [ ] **Step 1: Write the failing test**

`tests/core/tst_directorywatcher.cpp`:
```cpp
#include "core/directorywatcher.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;

namespace {

void append(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::Append));
    file.write(bytes);
}

} // namespace

// Port design §1 (FileEvents): inotify through QFileSystemWatcher, recursive, debounced.
class TestDirectoryWatcher : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void reportsNewFoldersFilesAndAppends() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        QVERIFY(QDir().mkpath(root + QStringLiteral("/p/sub")));
        QTRY_COMPARE(spy.size(), 1);
        append(root + QStringLiteral("/p/sub/s.jsonl"), "a\n");
        QTRY_COMPARE(spy.size(), 2);
        append(root + QStringLiteral("/p/sub/s.jsonl"), "b\n");
        QTRY_COMPARE(spy.size(), 3);
    }

    void aBurstIsOneChange() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        append(root + QStringLiteral("/s.jsonl"), "a\n");
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 200);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        for (int index = 0; index < 5; ++index) {
            append(root + QStringLiteral("/s.jsonl"), "x\n");
        }
        QTRY_COMPARE(spy.size(), 1);
        QTest::qWait(400);
        QCOMPARE(spy.size(), 1);
    }

    // SPEC §13: a root that appears later is watched from then on.
    void aRootCreatedLaterIsWatched() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        QVERIFY(QDir().mkpath(root));
        QTRY_VERIFY(spy.size() >= 1);
        const qsizetype before = spy.size();
        append(root + QStringLiteral("/s.jsonl"), "a\n");
        QTRY_VERIFY(spy.size() > before);
    }

    void otherFilesAreNotWatched() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        QVERIFY(QDir().mkpath(root));
        append(root + QStringLiteral("/notes.txt"), "a\n");
        DirectoryWatcher watcher({root}, QStringLiteral(".jsonl"), 50);
        QSignalSpy spy(&watcher, &DirectoryWatcher::changed);
        watcher.start();
        append(root + QStringLiteral("/notes.txt"), "b\n");
        QTest::qWait(300);
        QCOMPARE(spy.size(), 0);
    }
};

QTEST_GUILESS_MAIN(TestDirectoryWatcher)
#include "tst_directorywatcher.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(core/tst_directorywatcher.cpp
    TEST_NAME tst_directorywatcher
    LINK_LIBRARIES contribusage_core Qt6::Test)
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `core/directorywatcher.h: No such file or directory`.

- [ ] **Step 3: Write the watcher**

`src/core/directorywatcher.h`:
```cpp
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace contribusage {

// Port design §1 (FileEvents): watches `roots` recursively with inotify, every folder and every file ending in
// `suffix`, and coalesces the changes of `debounceMs` into one `changed()`. A root that does not exist yet is picked up
// when its parent folder changes (SPEC §13).
class DirectoryWatcher : public QObject {
    Q_OBJECT

public:
    DirectoryWatcher(QStringList roots, QString suffix, int debounceMs, QObject *parent = nullptr);

    void start();

Q_SIGNALS:
    void changed();

private:
    void watchRoots();
    void addTree(const QString &folder);
    void touched();

    QStringList m_roots;
    QString m_suffix;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

} // namespace contribusage
```

`src/core/directorywatcher.cpp`:
```cpp
#include "core/directorywatcher.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace contribusage {

DirectoryWatcher::DirectoryWatcher(QStringList roots, QString suffix, int debounceMs, QObject *parent)
    : QObject(parent), m_roots(std::move(roots)), m_suffix(std::move(suffix)) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(debounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &DirectoryWatcher::changed);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        watchRoots(); // new folders and files, and roots that appeared
        touched();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        // inotify drops a file that was replaced; watch the new one.
        if (QFileInfo::exists(path) && !m_watcher.files().contains(path)) {
            m_watcher.addPath(path);
        }
        touched();
    });
}

void DirectoryWatcher::start() {
    watchRoots();
}

void DirectoryWatcher::watchRoots() {
    for (const QString &root : std::as_const(m_roots)) {
        if (QFileInfo(root).isDir()) {
            addTree(root);
        } else if (const QString parent = QFileInfo(root).absolutePath(); QFileInfo(parent).isDir()) {
            if (!m_watcher.directories().contains(parent)) {
                m_watcher.addPath(parent);
            }
        }
    }
}

void DirectoryWatcher::addTree(const QString &folder) {
    const QStringList watchedFolders = m_watcher.directories();
    const QStringList watchedFiles = m_watcher.files();
    const QSet<QString> watched(watchedFolders.cbegin(), watchedFolders.cend());
    const QSet<QString> watchedFileSet(watchedFiles.cbegin(), watchedFiles.cend());
    QStringList add;
    if (!watched.contains(folder)) {
        add.append(folder);
    }
    QDirIterator it(folder, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        const QString path = info.filePath();
        if (info.isDir() ? !watched.contains(path)
                         : path.endsWith(m_suffix) && !watchedFileSet.contains(path)) {
            add.append(path);
        }
    }
    if (!add.isEmpty()) {
        m_watcher.addPaths(add);
    }
}

void DirectoryWatcher::touched() {
    m_debounce.start();
}

} // namespace contribusage
```

In `src/core/CMakeLists.txt`, add `directorywatcher.cpp directorywatcher.h` to the sources (after `apppaths.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R tst_directorywatcher`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/directorywatcher.* tests/core/tst_directorywatcher.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core tests
git commit -m "Core: recursive, debounced directory watcher over inotify

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Heatmap layers

**Files:**
- Modify: `src/core/heatmap.h`, `src/core/heatmap.cpp`, `tests/core/tst_heatmap.cpp`, `src/github/githubview.h`, `src/github/githubview.cpp`

**Interfaces:**
- Produces:
  - In `core/heatmap.h`: `struct HeatmapLayer { QString name; QString hue; QHash<QDate, int> levels; QHash<QDate, QString> texts; };` and `QVariantMap heatmapView(const QList<HeatmapLayer> &layers, QDate today)` (empty map for no layers).
  - In `github/githubview.h`: `std::optional<HeatmapLayer> githubLayer(const SourceState<GitHubReport> &)`; `githubHeatmap` keeps its signature and output.

- [ ] **Step 1: Write the failing test**

Add to `TestHeatmap` in `tests/core/tst_heatmap.cpp` (and `#include <QVariantMap>` at the top):
```cpp
    // FR-48: every layer's stripe and text per cell, in layer order; a day without text reads "no data".
    void heatmapViewStacksLayers() {
        const QDate today(2026, 10, 5);
        const HeatmapLayer claude{.name = QStringLiteral("Claude Code"),
                                  .hue = QStringLiteral("claude"),
                                  .levels = {{today, 4}},
                                  .texts = {{today, QStringLiteral("1.2M tokens")}}};
        const HeatmapLayer github{.name = QStringLiteral("GitHub"),
                                  .hue = QStringLiteral("github"),
                                  .levels = {{today, 2}, {today.addDays(-1), 0}},
                                  .texts = {{today, QStringLiteral("3 contributions")},
                                            {today.addDays(-1), QStringLiteral("0 contributions")}}};
        const QVariantMap view = heatmapView({claude, github}, today);
        QCOMPARE(view.value(QStringLiteral("layers")).toList().size(), qsizetype(2));
        QCOMPARE(view.value(QStringLiteral("columns")).toInt(), 26);
        const QVariantList cells = view.value(QStringLiteral("cells")).toList();
        const QVariantMap last = cells.last().toMap();
        QCOMPARE(last.value(QStringLiteral("line")).toString(),
                 QStringLiteral("2026-10-05 · Claude Code: 1.2M tokens · GitHub: 3 contributions"));
        const QVariantList stripes = last.value(QStringLiteral("stripes")).toList();
        QCOMPARE(stripes.size(), qsizetype(2));
        QCOMPARE(stripes.at(0).toMap().value(QStringLiteral("hue")).toString(), QStringLiteral("claude"));
        QCOMPARE(stripes.at(1).toMap().value(QStringLiteral("level")).toInt(), 2);
        const QVariantMap yesterday = cells.at(cells.size() - 2).toMap();
        QCOMPARE(yesterday.value(QStringLiteral("line")).toString(),
                 QStringLiteral("2026-10-04 · Claude Code: no data · GitHub: 0 contributions"));
        QVERIFY(yesterday.value(QStringLiteral("stripes")).toList().isEmpty());
        QVERIFY(heatmapView({}, today).isEmpty());
    }
```

- [ ] **Step 2: Run it and confirm it fails**

Run: `cmake --build build`
Expected: FAIL with `'HeatmapLayer' was not declared in this scope`.

- [ ] **Step 3: Write the layered view and move GitHub onto it**

In `src/core/heatmap.h`, add `#include <QString>` and `#include <QVariantMap>` and, before the closing namespace:
```cpp
// One source in the heatmap (FR-20, FR-48): its level per day (0 to 4) and the text of each day it has data for.
struct HeatmapLayer {
    QString name;
    QString hue; // the Theme color key: "claude", "github"
    QHash<QDate, int> levels;
    QHash<QDate, QString> texts;
};

// The heatmap block for QML: `layers` (name, hue), `columns` and one cell per heatmapDays() day with its stripes (one
// per layer with a level above 0, in layer order) and its line ("2026-10-05 · Claude Code: 1.2M tokens · GitHub: 3
// contributions", "no data" for a day a layer has no text for). An empty map without layers, so QML hides the block.
QVariantMap heatmapView(const QList<HeatmapLayer> &layers, QDate today);
```

In `src/core/heatmap.cpp`, add `#include <QVariantList>` and, before the closing namespace:
```cpp
QVariantMap heatmapView(const QList<HeatmapLayer> &layers, QDate today) {
    if (layers.isEmpty()) {
        return {};
    }
    QVariantList layerList;
    for (const HeatmapLayer &layer : layers) {
        layerList.append(QVariantMap{{QStringLiteral("name"), layer.name}, {QStringLiteral("hue"), layer.hue}});
    }
    const QList<QDate> days = heatmapDays(today);
    QVariantList cells;
    for (const QDate &date : days) {
        QString line = date.toString(Qt::ISODate);
        QVariantList stripes;
        for (const HeatmapLayer &layer : layers) {
            line += QStringLiteral(" · ") + layer.name + QStringLiteral(": ") +
                    layer.texts.value(date, QStringLiteral("no data"));
            if (const int level = layer.levels.value(date); level > 0) {
                stripes.append(QVariantMap{{QStringLiteral("hue"), layer.hue}, {QStringLiteral("level"), level}});
            }
        }
        cells.append(QVariantMap{{QStringLiteral("line"), line}, {QStringLiteral("stripes"), stripes}});
    }
    return {{QStringLiteral("layers"), layerList},
            {QStringLiteral("columns"), int((days.size() + 6) / 7)},
            {QStringLiteral("cells"), cells}};
}
```

In `src/github/githubview.h`, add `#include "core/heatmap.h"` and `#include <optional>`, and declare before `githubHeatmap`:
```cpp
// The GitHub layer of the heatmap: GitHub's own levels and "N contributions" per calendar day; nullopt without a
// snapshot.
std::optional<HeatmapLayer> githubLayer(const SourceState<GitHubReport> &state);
```

In `src/github/githubview.cpp`, replace the whole `githubHeatmap` function with:
```cpp
std::optional<HeatmapLayer> githubLayer(const SourceState<GitHubReport> &state) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    HeatmapLayer layer{.name = QStringLiteral("GitHub"), .hue = QStringLiteral("github"), .levels = {}, .texts = {}};
    for (const ContributionDay &day : snapshot->value.calendar.days()) {
        layer.levels.insert(day.date, day.level);
        layer.texts.insert(day.date, QString::number(day.count) + QStringLiteral(" contributions"));
    }
    return layer;
}

QVariantMap githubHeatmap(const SourceState<GitHubReport> &state, QDate today) {
    const auto layer = githubLayer(state);
    return layer ? heatmapView({*layer}, today) : QVariantMap();
}
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R "tst_heatmap|tst_githubview"`
Expected: PASS (the GitHub view's existing `heatmapLayer` test still passes unchanged).

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/core/heatmap.* src/github/githubview.* tests/core/tst_heatmap.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/core src/github tests
git commit -m "Core: heatmap view over several layers; GitHub becomes one layer

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Transcript lines and the live index

**Files:**
- Create: `src/claudecode/transcriptline.h`, `src/claudecode/transcriptline.cpp`, `src/claudecode/transcriptindex.h`, `src/claudecode/transcriptindex.cpp`, `tests/claudecode/tst_transcriptline.cpp`, `tests/claudecode/tst_transcriptindex.cpp`
- Modify: `src/claudecode/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `IncrementalJsonlReader` (Task 1), `ActivityDay`, `TokenCounts`.
- Produces (namespace `contribusage::claudecode`):
  - `struct TranscriptLine { QDateTime timestamp; QString sessionId, requestId, messageId, model; bool isSidechain; TokenCounts tokens; QString key() const; }`
  - `std::expected<std::optional<TranscriptLine>, QString> decodeTranscriptLine(QByteArrayView line)`
  - `QStringList transcriptRoots(const QString &home, const QString &configDir)`
  - `QStringList transcriptFiles(const QStringList &roots, const QString &probeFolder)`
  - `class TranscriptIndex { void update(const QStringList &files, const std::function<bool()> &interrupted = {}); QList<ActivityDay> days(const QTimeZone &zone) const; qint64 skippedLines() const; }`

- [ ] **Step 1: Write the failing tests**

`tests/claudecode/tst_transcriptline.cpp`:
```cpp
#include "claudecode/transcriptline.h"

#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {
// SPEC Appendix E, with the IDs and path scrubbed.
const QByteArray assistant =
    R"({"type":"assistant","isSidechain":false,"sessionId":"s-1","cwd":"/home/octocat/code","timestamp":"2026-09-27T12:34:56.789Z","requestId":"req_1","message":{"id":"msg_1","model":"claude-opus-5-5","role":"assistant","usage":{"input_tokens":12,"cache_creation_input_tokens":1834,"cache_read_input_tokens":145210,"output_tokens":421}}})";
} // namespace

// SPEC §8.3.2, FR-23, ported from TranscriptLineTests.swift.
class TestTranscriptLine : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void decodesAssistantLine() {
        const auto decoded = decodeTranscriptLine(assistant);
        QVERIFY(decoded && decoded->has_value());
        const TranscriptLine &line = **decoded;
        QCOMPARE(line.timestamp, QDateTime(QDate(2026, 9, 27), QTime(12, 34, 56, 789), QTimeZone::UTC));
        QCOMPARE(line.sessionId, QStringLiteral("s-1"));
        QCOMPARE(line.model, QStringLiteral("claude-opus-5-5"));
        QCOMPARE(line.isSidechain, false);
        QCOMPARE(line.tokens, (TokenCounts{.input = 12, .output = 421, .cacheWrite = 1834, .cacheRead = 145210}));
        QCOMPARE(line.key(), QStringLiteral("msg_1:req_1"));
    }

    void missingOptionalFieldsDefault() {
        const auto decoded = decodeTranscriptLine(
            R"({"type":"assistant","timestamp":"2026-09-27T12:34:56Z","message":{"model":"m","usage":{"output_tokens":5}}})");
        QVERIFY(decoded && decoded->has_value());
        QCOMPARE((*decoded)->tokens, (TokenCounts{.input = 0, .output = 5, .cacheWrite = 0, .cacheRead = 0}));
        QCOMPARE((*decoded)->key(), QString());
        QCOMPARE((*decoded)->sessionId, QString());
    }

    void keyFallsBackToTheMessageId() {
        const auto decoded = decodeTranscriptLine(
            R"({"type":"assistant","timestamp":"2026-09-27T12:34:56Z","message":{"id":"msg_9","model":"m","usage":{}}})");
        QVERIFY(decoded && decoded->has_value());
        QCOMPARE((*decoded)->key(), QStringLiteral("msg_9"));
    }

    void skipsLinesWithoutUsage_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("user") << QByteArray(R"({"type":"user","timestamp":"2026-09-27T12:00:00Z","message":{}})");
        QTest::newRow("no usage")
            << QByteArray(R"({"type":"assistant","timestamp":"2026-09-27T12:00:00Z","message":{"model":"m"}})");
        QTest::newRow("synthetic")
            << QByteArray(
                   R"({"type":"assistant","timestamp":"2026-09-27T12:00:00Z","message":{"model":"<synthetic>","usage":{}}})");
        QTest::newRow("summary with usage word") << QByteArray(R"({"type":"summary","summary":"usage"})");
    }
    void skipsLinesWithoutUsage() {
        QFETCH(QByteArray, line);
        const auto decoded = decodeTranscriptLine(line);
        QVERIFY(decoded);
        QVERIFY(!decoded->has_value());
    }

    void malformedLinesFail_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("not JSON") << QByteArray(R"({"type":"assistant","usage": )");
        QTest::newRow("no timestamp")
            << QByteArray(R"({"type":"assistant","message":{"model":"m","usage":{"input_tokens":1}}})");
        QTest::newRow("bad timestamp")
            << QByteArray(R"({"type":"assistant","timestamp":"yesterday","message":{"model":"m","usage":{}}})");
    }
    void malformedLinesFail() {
        QFETCH(QByteArray, line);
        QVERIFY(!decodeTranscriptLine(line));
    }
};

QTEST_GUILESS_MAIN(TestTranscriptLine)
#include "tst_transcriptline.moc"
```

`tests/claudecode/tst_transcriptindex.cpp`:
```cpp
#include "claudecode/transcriptindex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

QByteArray entry(const char *timestamp, const char *message, const char *request, const char *session,
                 const char *model = "claude-opus-5-5", int input = 10) {
    QByteArray line = R"({"type":"assistant","timestamp":")" + QByteArray(timestamp) + R"(","sessionId":")" +
                      QByteArray(session) + '"';
    if (request) {
        line += R"(,"requestId":")" + QByteArray(request) + '"';
    }
    line += R"(,"message":{)";
    if (message) {
        line += R"("id":")" + QByteArray(message) + R"(",)";
    }
    line += R"("model":")" + QByteArray(model) + R"(","usage":{"input_tokens":)" + QByteArray::number(input) +
            R"(,"output_tokens":1}}})";
    return line + '\n';
}

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(mode));
    file.write(bytes);
}

const QTimeZone utc = QTimeZone::UTC;

} // namespace

// SPEC §8.3.1, §8.3.3, FR-22, FR-24, FR-28; ported from TranscriptAggregatorTests, TranscriptFilesTests and
// TranscriptActivityTests.
class TestTranscriptIndex : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void rootsInOrder() {
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("/cfg/claude")),
                 (QStringList{QStringLiteral("/cfg/claude/projects"), QStringLiteral("/home/u/.claude/projects"),
                              QStringLiteral("/home/u/.config/claude/projects")}));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("/home/u/.claude/")),
                 (QStringList{QStringLiteral("/home/u/.claude/projects"),
                              QStringLiteral("/home/u/.config/claude/projects")}));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QString()).size(), qsizetype(2));
        QCOMPARE(transcriptRoots(QStringLiteral("/home/u"), QStringLiteral("relative")).size(), qsizetype(2));
    }

    // FR-28: the probe's project folder never counts; nested folders do.
    void filesSkipTheProbeProject() {
        QTemporaryDir dir;
        const QString root = dir.filePath(QStringLiteral("projects"));
        const QString probe = QStringLiteral("/home/u/.local/share/contribusage/providers/claude-code/probe");
        write(root + QStringLiteral("/-home-u-code/a.jsonl"), "x\n");
        write(root + QStringLiteral("/-home-u-code/sub/agent.jsonl"), "x\n");
        write(root + QStringLiteral("/-home-u-code/notes.txt"), "x\n");
        write(root + QStringLiteral("/-home-u--local-share-contribusage-providers-claude-code-probe/p.jsonl"), "x\n");
        QStringList files = transcriptFiles({root, dir.filePath(QStringLiteral("missing"))}, probe);
        files.sort();
        QCOMPARE(files, (QStringList{root + QStringLiteral("/-home-u-code/a.jsonl"),
                                     root + QStringLiteral("/-home-u-code/sub/agent.jsonl")}));
    }

    void duplicateKeyCountsOnce() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1") + entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        const auto days = index.days(utc);
        QCOMPARE(days.size(), qsizetype(1));
        QCOMPARE(days.at(0).requests, qint64(1));
        QCOMPARE(days.at(0).tokens.input, qint64(10));
    }

    // FR-24: message id alone is a key; a line without both counts every time.
    void keyFallsBackToMessageIdThenCountsEachLine() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", "m1", nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", nullptr, nullptr, "s1") +
                        entry("2026-10-01T10:00:00Z", nullptr, nullptr, "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(3));
    }

    void sessionsAreUniquePerDayAndModelsSplit() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1", "opus", 10) +
                        entry("2026-10-01T11:00:00Z", "m2", "r2", "s1", "sonnet", 20) +
                        entry("2026-10-01T12:00:00Z", "m3", "r3", "s2", "opus", 30) +
                        entry("2026-10-02T12:00:00Z", "m4", "r4", "s1", "opus", 40));
        TranscriptIndex index;
        index.update({file});
        const auto days = index.days(utc);
        QCOMPARE(days.size(), qsizetype(2));
        QCOMPARE(days.at(0).sessions, qint64(2));
        QCOMPARE(days.at(0).byModel.value(QStringLiteral("opus")).input, qint64(40));
        QCOMPARE(days.at(0).byModel.value(QStringLiteral("sonnet")).input, qint64(20));
        QCOMPARE(days.at(1).sessions, qint64(1));
    }

    // SPEC §8.3.3: the day is the local day of the given zone.
    void dayUsesTheZone() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T23:30:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.days(QTimeZone("Europe/Berlin")).at(0).day, QDate(2026, 10, 2));
        QCOMPARE(index.days(utc).at(0).day, QDate(2026, 10, 1));
    }

    // Review Focus 3: a resumed session repeats responses in a second file; the first file in path order counts.
    void firstDuplicateInPathOrderCounts() {
        QTemporaryDir dir;
        const QString first = dir.filePath(QStringLiteral("a.jsonl"));
        const QString second = dir.filePath(QStringLiteral("b.jsonl"));
        write(first, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1", "opus", 10));
        write(second, entry("2026-10-01T10:00:00Z", "m1", "r1", "s2", "opus", 99));
        TranscriptIndex index;
        index.update({second, first});
        const auto days = index.days(utc);
        QCOMPARE(days.at(0).requests, qint64(1));
        QCOMPARE(days.at(0).tokens.input, qint64(10));
        QCOMPARE(days.at(0).sessions, qint64(1));
    }

    void followsAppendsRewritesAndDeletes() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        write(file, entry("2026-10-01T11:00:00Z", "m2", "r2", "s1"), QIODevice::Append);
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(2));
        write(file, entry("2026-10-01T12:00:00Z", "m3", "r3", "s1")); // truncated and rewritten
        index.update({file});
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
        QVERIFY(QFile::remove(file));
        index.update({});
        QVERIFY(index.days(utc).isEmpty());
    }

    void countsSkippedLines() {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("a.jsonl"));
        write(file, QByteArray(R"({"type":"assistant","usage": )") + '\n' + entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        TranscriptIndex index;
        index.update({file});
        QCOMPARE(index.skippedLines(), qint64(1));
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
    }

    // Review Focus 5: an interruption stops between files.
    void interruptionStopsBetweenFiles() {
        QTemporaryDir dir;
        const QString first = dir.filePath(QStringLiteral("a.jsonl"));
        const QString second = dir.filePath(QStringLiteral("b.jsonl"));
        write(first, entry("2026-10-01T10:00:00Z", "m1", "r1", "s1"));
        write(second, entry("2026-10-01T10:00:00Z", "m2", "r2", "s1"));
        TranscriptIndex index;
        int asked = 0;
        index.update({first, second}, [&asked] { return ++asked > 1; });
        QCOMPARE(index.days(utc).at(0).requests, qint64(1));
    }
};

QTEST_GUILESS_MAIN(TestTranscriptIndex)
#include "tst_transcriptindex.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_transcriptline.cpp
    TEST_NAME tst_transcriptline
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)

ecm_add_test(claudecode/tst_transcriptindex.cpp
    TEST_NAME tst_transcriptindex
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `claudecode/transcriptline.h: No such file or directory`.

- [ ] **Step 3: Write the decoder and the index**

`src/claudecode/transcriptline.h`:
```cpp
#pragma once

#include "core/reports.h"

#include <QByteArrayView>
#include <QDateTime>
#include <QString>

#include <expected>
#include <optional>

namespace contribusage::claudecode {

// One usage-bearing line of a Claude Code transcript (SPEC §8.3.2, FR-23).
struct TranscriptLine {
    QDateTime timestamp;
    QString sessionId;
    QString requestId;
    QString messageId;
    QString model;
    bool isSidechain = false;
    TokenCounts tokens;

    // FR-24: `message.id:requestId`, `message.id` alone, or empty for a line without either.
    QString key() const;
};

// nullopt: a line that carries no usage (not `assistant`, no `message.usage`, a placeholder model such as
// `<synthetic>`). An error: not JSON, or no usable `timestamp`; the caller counts it as skipped (FR-23). A line without
// the text `"usage"` cannot carry usage and is not parsed at all, which keeps a scan of ~0.5 GB of transcripts short.
std::expected<std::optional<TranscriptLine>, QString> decodeTranscriptLine(QByteArrayView line);

} // namespace contribusage::claudecode
```

`src/claudecode/transcriptline.cpp`:
```cpp
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
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromRawData(line.data(), line.size()), &error);
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
```

`src/claudecode/transcriptindex.h`:
```cpp
#pragma once

#include "claudecode/transcriptline.h"
#include "core/jsonlreader.h"
#include "core/reports.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTimeZone>

#include <functional>

namespace contribusage::claudecode {

// FR-22 roots in order, without duplicates, whether they exist or not: `configDir/projects` when `configDir` (the login
// shell's CLAUDE_CONFIG_DIR) is an absolute path, then ~/.claude/projects and ~/.config/claude/projects.
QStringList transcriptRoots(const QString &home, const QString &configDir);

// `**/*.jsonl` under `roots`, without the project folder Claude Code would make for `probeFolder` (FR-28): every
// character outside [a-zA-Z0-9] of the working directory becomes `-` (SPEC §8.3.1).
QStringList transcriptFiles(const QStringList &roots, const QString &probeFolder);

// The live index (SPEC §8.3.4, ADR-024): per transcript its usage lines, one per key, and how many lines it could not
// decode. The days are rebuilt from all files, because a key repeats across files (resumed sessions) and a file can
// shrink or vanish.
class TranscriptIndex {
public:
    // Reads `files` and every file indexed before: new lines appended, rescanned files from scratch, vanished files
    // dropped. An unreadable file keeps what it had and is retried next time (SPEC §13). `interrupted` is asked before
    // each file; when it returns true the update stops there.
    void update(const QStringList &files, const std::function<bool()> &interrupted = {});

    // Per local day of `zone`, oldest first; a key counts once across files, the first in path order (FR-24, FR-25).
    QList<ActivityDay> days(const QTimeZone &zone) const;

    qint64 skippedLines() const;

private:
    // A usage line without the IDs its key holds; strings are shared through `m_strings` (NFR-2).
    struct Entry {
        QDateTime timestamp;
        QString sessionId;
        QString model;
        TokenCounts tokens;
    };
    struct Transcript {
        QHash<QString, Entry> keyed;
        QList<Entry> unkeyed;
        qint64 skipped = 0;
    };

    Entry entry(const TranscriptLine &line);

    IncrementalJsonlReader m_reader;
    QMap<QString, Transcript> m_files; // path order
    QSet<QString> m_strings;
};

} // namespace contribusage::claudecode
```

`src/claudecode/transcriptindex.cpp`:
```cpp
#include "claudecode/transcriptindex.h"

#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>

namespace contribusage::claudecode {

QStringList transcriptRoots(const QString &home, const QString &configDir) {
    QStringList roots;
    QStringList bases;
    if (configDir.startsWith(u'/')) {
        bases.append(configDir);
    }
    bases.append(home + QStringLiteral("/.claude"));
    bases.append(home + QStringLiteral("/.config/claude"));
    for (const QString &base : std::as_const(bases)) {
        const QString root = QDir::cleanPath(base + QStringLiteral("/projects"));
        if (!roots.contains(root)) {
            roots.append(root);
        }
    }
    return roots;
}

QStringList transcriptFiles(const QStringList &roots, const QString &probeFolder) {
    static const QRegularExpression outside(QStringLiteral("[^a-zA-Z0-9]"));
    QString probePath = QDir::cleanPath(probeFolder);
    const QString probeProject = probePath.replace(outside, QStringLiteral("-"));
    QStringList files;
    for (const QString &root : roots) {
        const QString skipped = root + u'/' + probeProject + u'/';
        QDirIterator it(root, {QStringLiteral("*.jsonl")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (!path.startsWith(skipped)) {
                files.append(path);
            }
        }
    }
    return files;
}

TranscriptIndex::Entry TranscriptIndex::entry(const TranscriptLine &line) {
    return {.timestamp = line.timestamp,
            .sessionId = *m_strings.insert(line.sessionId),
            .model = *m_strings.insert(line.model),
            .tokens = line.tokens};
}

void TranscriptIndex::update(const QStringList &files, const std::function<bool()> &interrupted) {
    QSet<QString> paths(files.cbegin(), files.cend());
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it) {
        paths.insert(it.key());
    }
    for (const QString &path : std::as_const(paths)) {
        if (interrupted && interrupted()) {
            return;
        }
        Transcript appended;
        const auto outcome = m_reader.read(path, [this, &appended](QByteArrayView text) {
            const auto decoded = decodeTranscriptLine(text);
            if (!decoded) {
                ++appended.skipped;
            } else if (decoded->has_value()) {
                const TranscriptLine &line = **decoded;
                const QString key = line.key();
                if (key.isEmpty()) {
                    appended.unkeyed.append(entry(line));
                } else if (!appended.keyed.contains(key)) {
                    appended.keyed.insert(key, entry(line));
                }
            }
        });
        if (!outcome) {
            continue; // SPEC §13: retried on the next change
        }
        switch (*outcome) {
        case IncrementalJsonlReader::Outcome::Gone:
            m_files.remove(path);
            break;
        case IncrementalJsonlReader::Outcome::Rescanned:
            m_files.insert(path, appended);
            break;
        case IncrementalJsonlReader::Outcome::Appended: {
            Transcript &file = m_files[path];
            for (auto it = appended.keyed.cbegin(); it != appended.keyed.cend(); ++it) {
                if (!file.keyed.contains(it.key())) {
                    file.keyed.insert(it.key(), it.value());
                }
            }
            file.unkeyed.append(appended.unkeyed);
            file.skipped += appended.skipped;
            break;
        }
        }
    }
}

QList<ActivityDay> TranscriptIndex::days(const QTimeZone &zone) const {
    struct Bucket {
        qint64 requests = 0;
        QSet<QString> sessions;
        TokenCounts tokens;
        QMap<QString, TokenCounts> byModel;
    };
    QMap<QDate, Bucket> buckets;
    const auto add = [&buckets, &zone](const Entry &entry) {
        Bucket &bucket = buckets[entry.timestamp.toTimeZone(zone).date()];
        ++bucket.requests;
        if (!entry.sessionId.isEmpty()) {
            bucket.sessions.insert(entry.sessionId);
        }
        bucket.tokens += entry.tokens;
        bucket.byModel[entry.model] += entry.tokens;
    };
    QSet<QString> seen;
    for (auto file = m_files.cbegin(); file != m_files.cend(); ++file) {
        for (auto it = file->keyed.cbegin(); it != file->keyed.cend(); ++it) {
            if (!seen.contains(it.key())) {
                seen.insert(it.key());
                add(it.value());
            }
        }
        for (const Entry &entry : file->unkeyed) {
            add(entry);
        }
    }
    QList<ActivityDay> days;
    for (auto it = buckets.cbegin(); it != buckets.cend(); ++it) {
        days.append({.day = it.key(),
                     .requests = it->requests,
                     .sessions = it->sessions.size(),
                     .tokens = it->tokens,
                     .byModel = it->byModel});
    }
    return days;
}

qint64 TranscriptIndex::skippedLines() const {
    qint64 skipped = 0;
    for (const Transcript &file : m_files) {
        skipped += file.skipped;
    }
    return skipped;
}

} // namespace contribusage::claudecode
```

In `src/claudecode/CMakeLists.txt`, add `transcriptindex.cpp transcriptindex.h transcriptline.cpp transcriptline.h` to the sources (after `limitsview.h`).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R "tst_transcriptline|tst_transcriptindex"`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/transcript* tests/claudecode/tst_transcript*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/claudecode tests
git commit -m "Claude Code: transcript decoding, file discovery and the live index

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: The activity job and its view

**Files:**
- Create: `src/claudecode/activityjob.h`, `src/claudecode/activityjob.cpp`, `src/claudecode/activityview.h`, `src/claudecode/activityview.cpp`, `tests/claudecode/tst_activityjob.cpp`, `tests/claudecode/tst_activityview.cpp`
- Modify: `src/claudecode/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `TranscriptIndex`, `transcriptRoots`, `transcriptFiles` (Task 5), `HistoryStore` (Task 2), `DirectoryWatcher` (Task 3), `HeatmapLayer` (Task 4), `ProcessRunning`, `SourceState`, `Snapshot`, `display.h`.
- Produces:
  - `class ActivityJob : public QObject` (header `claudecode/activityjob.h`): `ActivityJob(ProcessRunning &runner, QString home, QString shell, QString probeFolder, AppPaths paths, int debounceMs, QObject *parent = nullptr)`, `~ActivityJob()`, `const SourceState<ActivityReport> &state() const`, `void start()`, `void rescan()`, signal `stateChanged()`.
  - `QVariantMap activityView(const SourceState<ActivityReport> &, QDate today)` (keys `todayText`, `categoriesText`, `week`; empty without a snapshot) and `std::optional<HeatmapLayer> activityLayer(const SourceState<ActivityReport> &, QDate today)` (header `claudecode/activityview.h`).

- [ ] **Step 1: Write the failing tests**

`tests/claudecode/tst_activityjob.cpp`:
```cpp
#include "claudecode/activityjob.h"
#include "support/fakeprocessrunner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QString shell = QStringLiteral("/bin/sh");
using Phase = SourceState<ActivityReport>::Phase;

QByteArray entry(const QDateTime &timestamp, const char *message, int input) {
    return R"({"type":"assistant","timestamp":")" + timestamp.toUTC().toString(Qt::ISODateWithMs).toUtf8() +
           R"(","sessionId":"s1","requestId":"r","message":{"id":")" + QByteArray(message) +
           R"(","model":"opus","usage":{"input_tokens":)" + QByteArray::number(input) + "}}}\n";
}

void write(const QString &path, const QByteArray &bytes, QIODevice::OpenMode mode = QIODevice::WriteOnly) {
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(mode));
    file.write(bytes);
}

struct Fixture {
    QTemporaryDir temporary;
    FakeProcessRunner runner;

    QString home() const { return temporary.filePath(QStringLiteral("home")); }
    QString configDir() const { return temporary.filePath(QStringLiteral("cfg")); }
    QString transcript() const { return configDir() + QStringLiteral("/projects/-code/s.jsonl"); }
    AppPaths paths() const { return {.root = temporary.filePath(QStringLiteral("data"))}; }

    Fixture() { runner.answer(shell, QStringLiteral("-lc"), FakeProcessRunner::exited(0, configDir() + u'\n')); }

    std::unique_ptr<ActivityJob> job() {
        return std::make_unique<ActivityJob>(runner, home(), shell, paths().root + QStringLiteral("/probe"), paths(),
                                             50);
    }
};

qint64 inputOn(const ActivityJob &job, QDate day) {
    const auto snapshot = job.state().snapshot();
    if (!snapshot) {
        return -1;
    }
    for (const ActivityDay &item : snapshot->value.days) {
        if (item.day == day) {
            return item.tokens.input;
        }
    }
    return 0;
}

} // namespace

// SPEC §8.3, FR-22 to FR-29; ported from TranscriptActivityTests.swift.
class TestActivityJob : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // FR-22: CLAUDE_CONFIG_DIR from the login shell is a root.
    void reportsTheConfigDirTranscripts() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        write(fixture.transcript(), entry(now, "m1", 10));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(job->state().phase, Phase::Loaded);
        QCOMPARE(inputOn(*job, now.date()), qint64(10));
        QCOMPARE(job->state().snapshot()->origin, origins::activity());
        QCOMPARE(fixture.runner.requests.at(0).arguments.at(0), QStringLiteral("-lc"));
    }

    void withoutTranscriptsThereIsNoLocalData() {
        Fixture fixture;
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(job->state().phase, Phase::NotConfigured);
        QCOMPARE(job->state().reason, NotConfiguredReason::NoLocalData);
    }

    void followsChanges() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        write(fixture.transcript(), entry(now, "m1", 10));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(10));
        write(fixture.transcript(), entry(now, "m2", 5), QIODevice::Append);
        QTRY_COMPARE(inputOn(*job, now.date()), qint64(15));
    }

    // Review Focus 2: a day older than 48 h is frozen and survives its transcript.
    void frozenDaysSurviveDeletedTranscripts() {
        Fixture fixture;
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        write(fixture.transcript(), entry(old, "m1", 7));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        QVERIFY(QFile::remove(fixture.transcript()));
        job->rescan();
        QTest::qWait(200);
        QCOMPARE(inputOn(*job, old.date()), qint64(7));
        QVERIFY(QFile::exists(fixture.paths().providerFolder(QStringLiteral("claude-code")) +
                              QStringLiteral("/history.json")));
    }

    // Review Focus 4: a damaged history stays on disk; the live days still show.
    void damagedHistoryShowsLiveDays() {
        Fixture fixture;
        const QString history =
            fixture.paths().providerFolder(QStringLiteral("claude-code")) + QStringLiteral("/history.json");
        write(history, "damaged");
        const QDateTime old = QDateTime::currentDateTime().addDays(-10);
        write(fixture.transcript(), entry(old, "m1", 7));
        auto job = fixture.job();
        job->start();
        QTRY_COMPARE(inputOn(*job, old.date()), qint64(7));
        QFile file(history);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("damaged"));
    }

    // Review Focus 5: destroying the job while a scan runs neither crashes nor waits for the whole tree.
    void destroyedDuringAScan() {
        Fixture fixture;
        const QDateTime now = QDateTime::currentDateTime();
        for (int index = 0; index < 200; ++index) {
            write(fixture.configDir() + QStringLiteral("/projects/-code/s") + QString::number(index) +
                      QStringLiteral(".jsonl"),
                  entry(now, "m", index).repeated(200));
        }
        auto job = fixture.job();
        job->start();
        QTRY_VERIFY(!fixture.runner.requests.isEmpty());
        QTest::qWait(20);
        job.reset();
        QVERIFY(true);
    }
};

QTEST_GUILESS_MAIN(TestActivityJob)
#include "tst_activityjob.moc"
```

`tests/claudecode/tst_activityview.cpp`:
```cpp
#include "claudecode/activityview.h"
#include "core/display.h"

#include <QLocale>
#include <QTest>

using namespace contribusage;
using namespace contribusage::claudecode;

namespace {

const QDate today(2026, 10, 5);

ActivityDay day(QDate date, qint64 input, qint64 requests = 1, qint64 sessions = 1) {
    return {.day = date,
            .requests = requests,
            .sessions = sessions,
            .tokens = {.input = input, .output = 2, .cacheWrite = 3, .cacheRead = 4},
            .byModel = {}};
}

SourceState<ActivityReport> loaded(QList<ActivityDay> days) {
    return SourceState<ActivityReport>::loaded(
        {.value = {.provider = QStringLiteral("claude-code"), .days = std::move(days), .skippedLines = 0},
         .fetchedAt = QDateTime::currentDateTimeUtc(),
         .origin = origins::activity()});
}

} // namespace

// US-5, SPEC §11.2 activity block, FR-48 heatmap layer.
class TestActivityView : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); }

    void todayAndCategories() {
        const QVariantMap view = activityView(loaded({day(today.addDays(-1), 100), day(today, 1'000'000, 668, 8)}), today);
        QCOMPARE(view.value(QStringLiteral("todayText")).toString(),
                 QString(QStringLiteral("Today: 668 requests · 8 sessions · ") + compactCount(1'000'009)));
        QCOMPARE(view.value(QStringLiteral("categoriesText")).toString(),
                 QString(QStringLiteral("Input ") + compactCount(1'000'000) + QStringLiteral(" · Output ") +
                         compactCount(2) + QStringLiteral(" · Cache read ") + compactCount(4) +
                         QStringLiteral(" · Cache write ") + compactCount(3)));
    }

    void weekBarsEndWithToday() {
        const QVariantMap view = activityView(loaded({day(today.addDays(-6), 491), day(today, 991)}), today);
        const QVariantList week = view.value(QStringLiteral("week")).toList();
        QCOMPARE(week.size(), qsizetype(7));
        QCOMPARE(week.at(0).toMap().value(QStringLiteral("fraction")).toDouble(), 0.5);
        QCOMPARE(week.at(3).toMap().value(QStringLiteral("fraction")).toDouble(), 0.0);
        QCOMPARE(week.at(6).toMap().value(QStringLiteral("fraction")).toDouble(), 1.0);
        QVERIFY(week.at(6).toMap().value(QStringLiteral("today")).toBool());
        QVERIFY(!week.at(5).toMap().value(QStringLiteral("today")).toBool());
    }

    void noActivityTodayReadsZero() {
        const QVariantMap view = activityView(loaded({day(today.addDays(-2), 5)}), today);
        QCOMPARE(view.value(QStringLiteral("todayText")).toString(),
                 QString(QStringLiteral("Today: 0 requests · 0 sessions · ") + compactCount(0)));
    }

    void withoutASnapshotTheBlockIsHidden() {
        QVERIFY(activityView(SourceState<ActivityReport>::notConfigured(NotConfiguredReason::NoLocalData), today)
                    .isEmpty());
        QVERIFY(!activityLayer(SourceState<ActivityReport>::loading(std::nullopt), today));
    }

    // FR-48: quartile levels over the shown range, the busiest day at 4; texts in tokens.
    void layerUsesQuartilesAndTokenTexts() {
        const auto layer = activityLayer(loaded({day(today.addDays(-400), 9'999'999), day(today.addDays(-1), 91),
                                                 day(today, 991)}),
                                         today);
        QVERIFY(layer);
        QCOMPARE(layer->hue, QStringLiteral("claude"));
        QCOMPARE(layer->name, QStringLiteral("Claude Code"));
        QCOMPARE(layer->levels.value(today), 4);
        QVERIFY(layer->levels.value(today.addDays(-1)) < 4);
        QVERIFY(!layer->levels.contains(today.addDays(-400)));
        QCOMPARE(layer->texts.value(today), QString(compactCount(1000) + QStringLiteral(" tokens")));
    }
};

QTEST_GUILESS_MAIN(TestActivityView)
#include "tst_activityview.moc"
```

Append to `tests/CMakeLists.txt`:
```cmake

ecm_add_test(claudecode/tst_activityjob.cpp
    TEST_NAME tst_activityjob
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
target_include_directories(tst_activityjob PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

ecm_add_test(claudecode/tst_activityview.cpp
    TEST_NAME tst_activityview
    LINK_LIBRARIES contribusage_claudecode Qt6::Test)
```

- [ ] **Step 2: Run them and confirm they fail**

Run: `cmake -B build -G Ninja && cmake --build build`
Expected: FAIL with `claudecode/activityjob.h: No such file or directory`.

- [ ] **Step 3: Write the job and the view**

`src/claudecode/activityjob.h`:
```cpp
#pragma once

#include "claudecode/transcriptindex.h"
#include "core/apppaths.h"
#include "core/historystore.h"
#include "core/process.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <QObject>
#include <QThread>

#include <memory>
#include <optional>

namespace contribusage {
class DirectoryWatcher;
}

namespace contribusage::claudecode {

// Reads the transcripts on its own thread (SPEC §8.3, NFR: nothing blocks the plasmashell GUI thread). Lives in
// ActivityJob's worker thread; `scan()` runs there and hands its report back to the job's thread.
class ActivityWorker : public QObject {
    Q_OBJECT

public:
    ActivityWorker(QStringList roots, QString probeFolder, AppPaths paths);

    // Brings the index up to date, merges it with the frozen history and calls `done` (on this thread) with the report
    // and whether there is any local data at all.
    void scan(const std::function<void(ActivityReport, bool)> &done);

private:
    QStringList m_roots;
    QString m_probeFolder;
    AppPaths m_paths;
    TranscriptIndex m_index;
    std::optional<HistoryStore> m_history; // nullopt while history.json is unreadable: left untouched, live days shown
    bool m_historyOpened = false;
};

// Claude Code's activity source: resolves the roots (FR-22, CLAUDE_CONFIG_DIR from the login shell), watches them
// (debounced) and keeps the latest report as a SourceState for the view.
class ActivityJob : public QObject {
    Q_OBJECT

public:
    ActivityJob(ProcessRunning &runner, QString home, QString shell, QString probeFolder, AppPaths paths,
                int debounceMs, QObject *parent = nullptr);
    // Stops a running scan between two files and waits for the worker thread.
    ~ActivityJob() override;

    const SourceState<ActivityReport> &state() const { return m_state; }

    void start();
    void rescan();

Q_SIGNALS:
    void stateChanged();

private:
    void startWatching(const QString &configDir);
    void finish(ActivityReport report, bool hasData);

    ProcessRunning &m_runner;
    QString m_home;
    QString m_shell;
    QString m_probeFolder;
    AppPaths m_paths;
    int m_debounceMs;
    QThread m_thread;
    ActivityWorker *m_worker = nullptr; // lives in m_thread, deleted after it stops
    std::unique_ptr<DirectoryWatcher> m_watcher;
    bool m_scanning = false;
    bool m_scanAgain = false;
    SourceState<ActivityReport> m_state = SourceState<ActivityReport>::loading(std::nullopt);
};

} // namespace contribusage::claudecode
```

`src/claudecode/activityjob.cpp`:
```cpp
#include "claudecode/activityjob.h"

#include "core/directorywatcher.h"

#include <QPointer>

namespace contribusage::claudecode {

namespace {
const QString providerId = QStringLiteral("claude-code");
}

ActivityWorker::ActivityWorker(QStringList roots, QString probeFolder, AppPaths paths)
    : m_roots(std::move(roots)), m_probeFolder(std::move(probeFolder)), m_paths(std::move(paths)) {}

void ActivityWorker::scan(const std::function<void(ActivityReport, bool)> &done) {
    const QStringList files = transcriptFiles(m_roots, m_probeFolder);
    QThread *thread = QThread::currentThread();
    m_index.update(files, [thread] { return thread->isInterruptionRequested(); });
    if (thread->isInterruptionRequested()) {
        return;
    }
    if (!m_historyOpened) {
        m_historyOpened = true;
        if (auto opened = HistoryStore::open(providerId, m_paths)) {
            m_history = std::move(*opened);
        }
    }
    const QList<ActivityDay> live = m_index.days(QTimeZone::systemTimeZone());
    const QList<ActivityDay> days = m_history ? m_history->merge(live, QDateTime::currentDateTime()) : live;
    done({.provider = providerId, .days = days, .skippedLines = m_index.skippedLines()},
         !files.isEmpty() || !days.isEmpty());
}

ActivityJob::ActivityJob(ProcessRunning &runner, QString home, QString shell, QString probeFolder, AppPaths paths,
                         int debounceMs, QObject *parent)
    : QObject(parent), m_runner(runner), m_home(std::move(home)), m_shell(std::move(shell)),
      m_probeFolder(std::move(probeFolder)), m_paths(std::move(paths)), m_debounceMs(debounceMs) {}

ActivityJob::~ActivityJob() {
    m_thread.requestInterruption();
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
}

void ActivityJob::start() {
    m_state = SourceState<ActivityReport>::loading(std::nullopt);
    Q_EMIT stateChanged();
    // CLAUDE_CONFIG_DIR from the login shell; profile scripts may print first, so the value is the last line, and only an
    // absolute path counts.
    const ProcessRequest request{.executable = m_shell,
                                 .arguments = {QStringLiteral("-lc"),
                                               QStringLiteral("printf '%s\\n' \"$CLAUDE_CONFIG_DIR\"")},
                                 .workingDirectory = m_home,
                                 .environment = std::nullopt,
                                 .timeoutMs = 10'000};
    m_runner.run(request, this, [this](ProcessOutcome outcome) {
        QString configDir;
        if (outcome && outcome->exitCode == 0) {
            const QStringList lines = outcome->stdoutText.split(u'\n', Qt::SkipEmptyParts);
            if (!lines.isEmpty() && lines.last().startsWith(u'/')) {
                configDir = lines.last();
            }
        }
        startWatching(configDir);
    });
}

void ActivityJob::startWatching(const QString &configDir) {
    const QStringList roots = transcriptRoots(m_home, configDir);
    m_worker = new ActivityWorker(roots, m_probeFolder, m_paths);
    m_worker->moveToThread(&m_thread);
    m_thread.start(QThread::LowPriority); // FR-27
    m_watcher = std::make_unique<DirectoryWatcher>(roots, QStringLiteral(".jsonl"), m_debounceMs);
    connect(m_watcher.get(), &DirectoryWatcher::changed, this, &ActivityJob::rescan);
    m_watcher->start();
    rescan();
}

void ActivityJob::rescan() {
    if (!m_worker) {
        return;
    }
    if (m_scanning) {
        m_scanAgain = true;
        return;
    }
    m_scanning = true;
    const QPointer<ActivityJob> job = this;
    ActivityWorker *worker = m_worker;
    QMetaObject::invokeMethod(m_worker, [job, worker] {
        worker->scan([job](ActivityReport report, bool hasData) {
            // Back on the job's thread; a job destroyed meanwhile drops the event with itself.
            QMetaObject::invokeMethod(job.data(), [job, report = std::move(report), hasData]() mutable {
                if (job) {
                    job->finish(std::move(report), hasData);
                }
            });
        });
    });
}

void ActivityJob::finish(ActivityReport report, bool hasData) {
    m_scanning = false;
    if (hasData) {
        m_state = SourceState<ActivityReport>::loaded(
            {.value = std::move(report), .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::activity()});
    } else {
        m_state = SourceState<ActivityReport>::notConfigured(NotConfiguredReason::NoLocalData);
    }
    Q_EMIT stateChanged();
    if (m_scanAgain) {
        m_scanAgain = false;
        rescan();
    }
}

} // namespace contribusage::claudecode
```

`src/claudecode/activityview.h`:
```cpp
#pragma once

#include "core/heatmap.h"
#include "core/reports.h"
#include "core/snapshot.h"

#include <QDate>
#include <QVariantMap>

#include <optional>

namespace contribusage::claudecode {

// US-5, SPEC §11.2: `todayText` ("Today: 668 requests · 8 sessions · 4.2M"), `categoriesText` (the four token
// categories) and `week` (seven bars, oldest first, `fraction` of the busiest day and `today` on the last one). An empty
// map without a snapshot, so QML hides the block.
QVariantMap activityView(const SourceState<ActivityReport> &state, QDate today);

// The Claude Code layer of the heatmap (FR-48): total tokens per day as quartile levels over the shown range, "1.2M
// tokens" as the text; nullopt without a snapshot.
std::optional<HeatmapLayer> activityLayer(const SourceState<ActivityReport> &state, QDate today);

} // namespace contribusage::claudecode
```

`src/claudecode/activityview.cpp`:
```cpp
#include "claudecode/activityview.h"

#include "core/display.h"

#include <QVariantList>

namespace contribusage::claudecode {

QVariantMap activityView(const SourceState<ActivityReport> &state, QDate today) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return {};
    }
    QHash<QDate, ActivityDay> byDay;
    for (const ActivityDay &day : snapshot->value.days) {
        byDay.insert(day.day, day);
    }
    const ActivityDay now = byDay.value(today);
    QList<qint64> totals;
    for (int offset = 6; offset >= 0; --offset) {
        totals.append(byDay.value(today.addDays(-offset)).tokens.total());
    }
    const QList<double> fractions = barFractions(totals);
    QVariantList week;
    for (qsizetype index = 0; index < fractions.size(); ++index) {
        week.append(QVariantMap{{QStringLiteral("fraction"), fractions.at(index)},
                                {QStringLiteral("today"), index == fractions.size() - 1}});
    }
    return {{QStringLiteral("todayText"),
             QString(QStringLiteral("Today: ") + QString::number(now.requests) + QStringLiteral(" requests · ") +
                     QString::number(now.sessions) + QStringLiteral(" sessions · ") +
                     compactCount(now.tokens.total()))},
            {QStringLiteral("categoriesText"),
             QString(QStringLiteral("Input ") + compactCount(now.tokens.input) + QStringLiteral(" · Output ") +
                     compactCount(now.tokens.output) + QStringLiteral(" · Cache read ") +
                     compactCount(now.tokens.cacheRead) + QStringLiteral(" · Cache write ") +
                     compactCount(now.tokens.cacheWrite))},
            {QStringLiteral("week"), week}};
}

std::optional<HeatmapLayer> activityLayer(const SourceState<ActivityReport> &state, QDate today) {
    const auto snapshot = state.snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    const QList<QDate> shown = heatmapDays(today);
    const QDate first = shown.first();
    HeatmapLayer layer{.name = QStringLiteral("Claude Code"), .hue = QStringLiteral("claude"), .levels = {}, .texts = {}};
    QHash<QDate, qint64> values;
    for (const ActivityDay &day : snapshot->value.days) {
        if (day.day >= first && day.day <= today) {
            values.insert(day.day, day.tokens.total());
            layer.texts.insert(day.day, compactCount(day.tokens.total()) + QStringLiteral(" tokens"));
        }
    }
    layer.levels = quartileLevels(values);
    return layer;
}

} // namespace contribusage::claudecode
```

In `src/claudecode/CMakeLists.txt`, add `activityjob.cpp activityjob.h activityview.cpp activityview.h` to the sources (at the top of the list).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R "tst_activityjob|tst_activityview"`
Expected: PASS.

- [ ] **Step 5: Format, full suite, commit**

Run: `clang-format -i src/claudecode/activity* tests/claudecode/tst_activity*.cpp && clang-format --dry-run --Werror $(git ls-files -co --exclude-standard '*.cpp' '*.h') && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: no format output; all tests pass.

```bash
git add src/claudecode tests
git commit -m "Claude Code: activity job on a worker thread, with history, and its view

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Activity in the widget

**Files:**
- Modify: `src/applet/popupmodel.h`, `src/applet/popupmodel.cpp`

**Interfaces:**
- Consumes: `ActivityJob`, `activityView`, `activityLayer` (Task 6), `githubLayer`, `heatmapView` (Task 4).
- Produces: in live mode, the Claude Code provider map gains `activity` when there is a snapshot, and `heatmap` holds the Claude Code layer first, then the GitHub layer.

- [ ] **Step 1: Pin the combined heatmap in a test**

The applet's live wiring has no seam for tests (it builds the real runner, wallet and network), so the behaviour it adds is pinned at the level of the functions it composes. Add to `TestActivityView` in `tests/claudecode/tst_activityview.cpp` (and `#include "github/githubview.h"`; add `contribusage_github` to the test's `LINK_LIBRARIES` in `tests/CMakeLists.txt`):
```cpp
    // The widget's heatmap: Claude Code first, then GitHub, in one block.
    void layersCombineWithGitHub() {
        using GitHubState = SourceState<github::GitHubReport>;
        const github::GitHubReport report{
            .calendar = {.login = QStringLiteral("octocat"),
                         .weeks = {{{.date = today, .count = 3, .level = 2}}},
                         .totalContributions = 3},
            .stats = {}};
        const auto githubState = GitHubState::loaded(
            {.value = report, .fetchedAt = QDateTime::currentDateTimeUtc(), .origin = origins::github()});
        QList<HeatmapLayer> layers;
        for (const auto &layer : {activityLayer(loaded({day(today, 991)}), today), github::githubLayer(githubState)}) {
            if (layer) {
                layers.append(*layer);
            }
        }
        const QVariantMap view = heatmapView(layers, today);
        QCOMPARE(view.value(QStringLiteral("cells")).toList().last().toMap().value(QStringLiteral("line")).toString(),
                 QString(QStringLiteral("2026-10-05 · Claude Code: ") + compactCount(1000) +
                         QStringLiteral(" tokens · GitHub: 3 contributions")));
    }
```

- [ ] **Step 2: Run it**

Run: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure -R tst_activityview`
Expected: PASS (the composed functions exist from Tasks 4 and 6; this test fixes the composition the widget uses).

- [ ] **Step 3: Wire the widget**

`src/applet/popupmodel.h`:
- add `class ActivityJob;` to `namespace claudecode` in the forward declarations
- add the member `std::unique_ptr<contribusage::claudecode::ActivityJob> m_activityJob;` after `m_job`

`src/applet/popupmodel.cpp`:
- add the includes `claudecode/activityjob.h` and `claudecode/activityview.h`
- in the live branch of the constructor, after `m_job->start();`, add:
```cpp
    m_activityJob = std::make_unique<claudecode::ActivityJob>(
        *m_runner, QDir::homePath(), shell, m_limits->probeFolder(), AppPaths::live(), 5'000); // SPEC §8.3, 5 s debounce
    connect(m_activityJob.get(), &claudecode::ActivityJob::stateChanged, this, &PopupModel::changed);
    m_activityJob->start();
```
- in the destructor, as its first line: `m_activityJob.reset();`
- replace `PopupModel::heatmap()` with:
```cpp
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
```
- replace the live return of `PopupModel::providers()` with:
```cpp
    QVariantMap provider = claudecode::limitsView(m_job->state(), QDateTime::currentDateTime());
    const QVariantMap activity = claudecode::activityView(m_activityJob->state(), QDate::currentDate());
    if (!activity.isEmpty()) {
        provider.insert(QStringLiteral("activity"), activity);
    }
    return {provider};
```
(`ClaudeCodeLimits::probeFolder()` is public; if it is not, make it public: the probe folder is the one FR-28 excludes.)

- [ ] **Step 4: Run every test and look at it**

Run: `cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
Expected: all tests pass.

Restart the viewer (`for p in $(pgrep -x plasmoidviewer); do kill $p; done`, then `QT_PLUGIN_PATH=$PWD/build/bin plasmoidviewer -a org.contribusage.widget -f planar -s 420x900`), wait 15 s, take a screenshot.
Expected: the Claude Code group shows "On this machine", the Today line, the categories and seven bars; the heatmap shows the Claude Code legend and stripes next to GitHub's; no QML warnings in the viewer's output; plasmoidviewer stays responsive while the first scan runs.

Cross-check today's numbers independently:
```bash
python3 - <<'EOF'
import json, glob, os, datetime
seen=set(); req=0; tok=0
today=datetime.date.today()
for f in sorted(glob.glob(os.path.expanduser('~/.claude/projects/**/*.jsonl'), recursive=True)):
    if '-contribusage-providers-claude-code-probe' in f: continue
    for line in open(f, errors='replace'):
        if '"usage"' not in line: continue
        try: o=json.loads(line)
        except Exception: continue
        m=o.get('message') or {}
        if o.get('type')!='assistant' or not isinstance(m.get('usage'),dict): continue
        model=m.get('model') or ''
        if not model or (model.startswith('<') and model.endswith('>')): continue
        ts=o.get('timestamp')
        if not ts: continue
        d=datetime.datetime.fromisoformat(ts.replace('Z','+00:00')).astimezone().date()
        if d!=today: continue
        k=(m.get('id') or '') + (':'+o['requestId'] if m.get('id') and o.get('requestId') else '')
        if k:
            if k in seen: continue
            seen.add(k)
        u=m['usage']; req+=1
        tok+=sum(u.get(x) or 0 for x in ('input_tokens','output_tokens','cache_creation_input_tokens','cache_read_input_tokens'))
print(req, tok)
EOF
```
Expected: the request count equals the widget's "Today: N requests" (taken a moment apart, so allow the few requests made in between), and the token total matches its compact count.

- [ ] **Step 5: Format and commit**

```bash
clang-format -i src/applet/*.cpp src/applet/*.h tests/claudecode/*.cpp
git add src/applet tests
git commit -m "Applet: Claude Code activity and its heatmap layer from local transcripts

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 8: Contract and vault

**Files:**
- Create: `llm-wiki/decisions/0044-transcripts-on-a-worker-thread.md`
- Modify: `SPEC.md` (§17.8), `llm-wiki/modules/core.md`, `llm-wiki/modules/claude-code.md`, `llm-wiki/modules/applet.md`, `llm-wiki/index.md`, `llm-wiki/log.md`

- [ ] **Step 1: Contract**

In `SPEC.md` §17.8:
- replace the `L-1b` line with:
```markdown
- [x] **L-1b** File events (`DirectoryWatcher`: inotify through `QFileSystemWatcher`, recursive, debounced), the incremental JSONL reader and the history store, in the core; the secret store (KWallet) came with L-3. The `FileReading` seam with the declared-roots check waits for a second activity provider (ADR-044). Accept: `tst_jsonlreader`, `tst_historystore` and `tst_directorywatcher` pass.
```
- replace the `L-2b` line with:
```markdown
- [x] **L-2b** Claude Code activity from local transcripts (§8.3, FR-22 to FR-29): roots from the login shell's CLAUDE_CONFIG_DIR, the probe project excluded, de-duplication across files, the live index read on a worker thread (ADR-044), frozen history in `history.json`, the activity block and the Claude Code heatmap layer. Accept: `tst_transcriptline`, `tst_transcriptindex`, `tst_activityjob` and `tst_activityview` pass; the widget's numbers for today match an independent count over the same transcripts.
```

- [ ] **Step 2: ADR-044**

`llm-wiki/decisions/0044-transcripts-on-a-worker-thread.md` (today's date for both `YYYY-MM-DD`):
```markdown
---
type: decision
status: accepted
updated: YYYY-MM-DD
aliases: [ADR-044]
tracks: [src/claudecode/activityjob.cpp, src/claudecode/transcriptline.cpp]
tags: [performance, activity, threading]
---
# ADR-044: Transcripts are read on a worker thread

- **Decided:** YYYY-MM-DD · **Spec:** SPEC §8.3, FR-26, FR-27, SPEC §2 (nothing blocks the plasmashell GUI thread) · **Supersedes:** —

## Context
The widget runs inside plasmashell. Building the live index reads every transcript at start (ADR-024), which is several hundred MB on an active machine; parsing that on the GUI thread would freeze the whole desktop shell for seconds.

## Options
| Option | For | Against |
|---|---|---|
| Read on the GUI thread in slices | No threads | Still long stalls per large file; complex resumption |
| A worker `QThread` owning the index and the history store | The GUI thread only receives finished reports; the index stays single-threaded | A thread per widget instance; shutdown must stop a scan |
| A separate helper process | Isolation | Another binary, IPC, packaging |

## Decision
`ActivityJob` owns a low-priority worker thread. The `ActivityWorker` there owns the `TranscriptIndex` and the `HistoryStore` and is the only code touching them; scans are requested by a queued call and their reports come back by a queued call. Destruction asks the thread to stop between two files and waits for it. Lines without the text `"usage"` are not parsed, so a cold scan stays short; such lines are not counted as skipped even when malformed.

## Consequences
- The `FileReading` seam of SPEC §10.6 is not built yet: tests use real temporary files, and the declared-roots conformance check (§16.4) arrives with a second activity provider.
- The skipped-lines diagnostic counts only lines that mention `"usage"`.
```

- [ ] **Step 3: Vault**

- `llm-wiki/modules/core.md`: `updated:` today; add `src/core/jsonlreader`, `historystore`, `directorywatcher` and `heatmapView` to the module's contents list with one line each (what they do, the spec section).
- `llm-wiki/modules/claude-code.md`: `updated:` today; add `transcriptline`, `transcriptindex`, `activityjob` (worker thread, ADR-044) and `activityview`; set its tracks to include `src/claudecode` and `tests/claudecode` if they are not there yet.
- `llm-wiki/modules/applet.md`: `updated:` today; in `## Contract` change the PopupModel bullet to say the live Claude Code provider carries `activity` and the heatmap stacks the Claude Code and GitHub layers since L-2b.
- `llm-wiki/index.md`: under Decisions, after ADR-043, add `- [[decisions/0044-transcripts-on-a-worker-thread]]: ADR-044, transcripts are read on a worker thread; lines without "usage" are not parsed`.
- `llm-wiki/log.md`, append:
```markdown

## [YYYY-MM-DD] feat | Claude Code activity (L-1b, L-2b)
- `src/core/`: incremental JSONL reader, history store, recursive debounced directory watcher, layered heatmap view.
- `src/claudecode/`: transcript decoding, discovery and live index on a worker thread (ADR-044); the widget shows today, the week and the Claude Code heatmap layer.
```

- [ ] **Step 4: Run every check and commit**

Run: `llm-wiki/tools/wiki.sh lint && cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure && clang-format --dry-run --Werror $(git ls-files '*.cpp' '*.h') && echo all-ok`
Expected: `wiki lint: clean`, all tests pass, then `all-ok`.

```bash
git add SPEC.md llm-wiki
git commit -m "Spec: L-1b and L-2b done; ADR-044 transcripts on a worker thread

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
