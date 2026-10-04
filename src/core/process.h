#pragma once

#include "core/reports.h"

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <expected>
#include <functional>
#include <optional>

namespace contribusage {

struct ProcessRequest {
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    std::optional<QProcessEnvironment> environment; // nullopt inherits the widget's environment
    int timeoutMs = 30'000;
};

struct ProcessResult {
    int exitCode = 0;
    QString stdoutText;
    QString stderrText;

    bool operator==(const ProcessResult &) const = default;
};

using ProcessOutcome = std::expected<ProcessResult, SourceError>;
using ProcessCallback = std::function<void(ProcessOutcome)>;

// SPEC §10.6: runs a child process without blocking the caller. `done` is called exactly once, on the caller's thread,
// with the result, `TimedOut`, or `Io` when the executable cannot start.
class ProcessRunning {
public:
    virtual ~ProcessRunning() = default;
    virtual void run(const ProcessRequest &request, ProcessCallback done) = 0;
};

// FR-7, NFR-18: stdin /dev/null, both streams captured, the timeout sends SIGTERM and SIGKILL 2 s later. At most one
// process runs at a time across all instances; later requests wait their turn.
class LiveProcessRunner final : public ProcessRunning {
public:
    void run(const ProcessRequest &request, ProcessCallback done) override;
};

} // namespace contribusage
