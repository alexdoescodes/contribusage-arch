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
