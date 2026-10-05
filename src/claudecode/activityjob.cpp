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
    const bool complete = m_index.update(files, [thread] { return thread->isInterruptionRequested(); });
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
    // A day freezes only from a scan that read every file; otherwise its count may be short for good.
    const QList<ActivityDay> days = !m_history ? live
                                    : complete ? m_history->merge(live, QDateTime::currentDateTime())
                                               : m_history->shown(live);
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
    // CLAUDE_CONFIG_DIR from the login shell; profile scripts may print first, so the value is the last line, and only
    // an absolute path counts.
    const ProcessRequest request{
        .executable = m_shell,
        .arguments = {QStringLiteral("-lc"), QStringLiteral("printf '%s\\n' \"$CLAUDE_CONFIG_DIR\"")},
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
