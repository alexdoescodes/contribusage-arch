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
        if (info.isDir() ? !watched.contains(path) : path.endsWith(m_suffix) && !watchedFileSet.contains(path)) {
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
