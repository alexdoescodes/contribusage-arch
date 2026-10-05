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
    // US-12: deletes history.json here, on the thread that writes it.
    void deleteHistory();

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
    // US-12: deletes the frozen history on the worker thread, then rescans: days whose transcripts are gone are gone.
    void deleteHistory();
    // FR-46: inactive, the job neither watches nor scans; active again, it rescans.
    void setActive(bool active);

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
    bool m_active = true;
    bool m_scanning = false;
    bool m_scanAgain = false;
    SourceState<ActivityReport> m_state = SourceState<ActivityReport>::loading(std::nullopt);
};

} // namespace contribusage::claudecode
