#pragma once

#include "core/process.h"

#include <QList>
#include <QPointer>
#include <QTimer>

// A scripted ProcessRunning for tests: the first rule whose executable and first argument match answers; every request
// is recorded. An unmatched request fails with Io, like a missing executable. Answers arrive on the next event loop
// pass, as the live runner's do.
class FakeProcessRunner final : public contribusage::ProcessRunning {
public:
    using ProcessRunning::run;

    struct Rule {
        QString executable;
        QString firstArgument; // empty matches any
        contribusage::ProcessOutcome outcome;
    };

    void answer(const QString &executable, const QString &firstArgument, contribusage::ProcessOutcome outcome) {
        rules.append({.executable = executable, .firstArgument = firstArgument, .outcome = std::move(outcome)});
    }

    static contribusage::ProcessOutcome exited(int code, const QString &stdoutText, const QString &stderrText = {}) {
        return contribusage::ProcessResult{.exitCode = code, .stdoutText = stdoutText, .stderrText = stderrText};
    }

    void run(const contribusage::ProcessRequest &request, QObject *context,
             contribusage::ProcessCallback done) override {
        requests.append(request);
        contribusage::ProcessOutcome outcome = std::unexpected(
            contribusage::SourceError{.kind = contribusage::SourceError::Kind::Io, .text = QStringLiteral("no rule")});
        for (const Rule &rule : rules) {
            if (rule.executable == request.executable &&
                (rule.firstArgument.isEmpty() || request.arguments.value(0) == rule.firstArgument)) {
                outcome = rule.outcome;
                break;
            }
        }
        const QPointer<QObject> guard = context;
        const bool hasContext = context != nullptr;
        QTimer::singleShot(0, [guard, hasContext, outcome, done = std::move(done)]() {
            if (!hasContext || guard) {
                done(outcome);
            }
        });
    }

    QList<Rule> rules;
    QList<contribusage::ProcessRequest> requests;
};
