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
    struct stat info{};
    if (::fstat(file.handle(), &info) != 0) {
        return std::unexpected(QStringLiteral("cannot stat ") + path);
    }
    const auto previous = m_files.constFind(path);
    const bool known = previous != m_files.cend();
    const bool rescan = known && (previous->device != quint64(info.st_dev) || previous->inode != quint64(info.st_ino) ||
                                  qint64(info.st_size) < previous->offset);
    State state =
        known && !rescan
            ? *previous
            : State{.device = quint64(info.st_dev), .inode = quint64(info.st_ino), .offset = 0, .pending = {}};
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
