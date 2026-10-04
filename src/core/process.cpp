#include "core/process.h"

#include <QList>
#include <QObject>
#include <QProcess>
#include <QTimer>

#include <memory>
#include <utility>

namespace contribusage {

namespace {

struct Job {
    ProcessRequest request;
    ProcessCallback done;
};

// NFR-18: one child process at a time, first come first served.
class ProcessQueue : public QObject {
public:
    static ProcessQueue &instance() {
        static ProcessQueue queue;
        return queue;
    }

    void enqueue(Job job) {
        m_jobs.append(std::move(job));
        if (!m_busy) {
            startNext();
        }
    }

private:
    void startNext();

    QList<Job> m_jobs;
    bool m_busy = false;
};

void ProcessQueue::startNext() {
    if (m_jobs.isEmpty()) {
        m_busy = false;
        return;
    }
    m_busy = true;
    Job job = m_jobs.takeFirst();

    auto *process = new QProcess(this);
    process->setProgram(job.request.executable);
    process->setArguments(job.request.arguments);
    process->setWorkingDirectory(job.request.workingDirectory);
    if (job.request.environment) {
        process->setProcessEnvironment(*job.request.environment);
    }
    process->setStandardInputFile(QProcess::nullDevice());

    auto *deadline = new QTimer(process);
    deadline->setSingleShot(true);
    auto timedOut = std::make_shared<bool>(false);
    auto done = std::make_shared<ProcessCallback>(std::move(job.done));

    // Reports once, frees the process, then starts the next job.
    auto complete = [this, process, deadline, done](ProcessOutcome outcome) {
        if (!*done) {
            return;
        }
        deadline->stop();
        const ProcessCallback callback = std::exchange(*done, nullptr);
        process->deleteLater();
        callback(std::move(outcome));
        startNext();
    };

    connect(deadline, &QTimer::timeout, process, [process, timedOut] {
        *timedOut = true;
        process->terminate();
        QTimer::singleShot(2'000, process, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    });
    connect(process, &QProcess::errorOccurred, this, [process, complete](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            complete(std::unexpected(SourceError{.kind = SourceError::Kind::Io, .text = process->errorString()}));
        }
    });
    connect(process, &QProcess::finished, this, [process, timedOut, complete](int exitCode, QProcess::ExitStatus) {
        if (*timedOut) {
            complete(std::unexpected(SourceError{.kind = SourceError::Kind::TimedOut}));
            return;
        }
        complete(ProcessResult{.exitCode = exitCode,
                               .stdoutText = QString::fromUtf8(process->readAllStandardOutput()),
                               .stderrText = QString::fromUtf8(process->readAllStandardError())});
    });

    // The deadline starts first: a launch failure can report before start() returns.
    deadline->start(job.request.timeoutMs);
    process->start();
}

} // namespace

void LiveProcessRunner::run(const ProcessRequest &request, ProcessCallback done) {
    ProcessQueue::instance().enqueue(Job{.request = request, .done = std::move(done)});
}

} // namespace contribusage
