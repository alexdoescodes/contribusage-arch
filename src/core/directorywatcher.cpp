#include "core/directorywatcher.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

namespace contribusage {

namespace {
Q_LOGGING_CATEGORY(watcherLog, "contribusage.watcher")
}

DirectoryWatcher::DirectoryWatcher(QStringList roots, QString suffix, int debounceMs, QObject *parent)
    : QObject(parent), m_roots(std::move(roots)), m_suffix(std::move(suffix)) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(debounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &DirectoryWatcher::changed);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        if (m_parents.contains(path)) {
            watchRoots(); // a root may have appeared
        } else if (QFileInfo(path).isDir()) {
            addTree(path); // new folders and files in this folder only
        } else {
            m_folders.remove(path); // inotify dropped it with the folder
        }
        touched();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        // inotify drops a file that was removed or replaced; watch a replacement, forget a removal.
        m_files.remove(path);
        if (QFileInfo::exists(path)) {
            watch({path});
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
            if (!m_folders.contains(root)) {
                addTree(root);
            }
        } else if (const QString parent = QFileInfo(root).absolutePath();
                   QFileInfo(parent).isDir() && !m_parents.contains(parent)) {
            m_parents.insert(parent);
            m_watcher.addPath(parent);
        }
    }
}

void DirectoryWatcher::addTree(const QString &folder) {
    QStringList add;
    QStringList newFolders;
    if (!m_folders.contains(folder)) {
        add.append(folder);
    }
    const QFileInfoList children =
        QDir(folder).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo &info : children) {
        const QString path = info.filePath();
        if (info.isDir()) {
            if (!m_folders.contains(path)) {
                newFolders.append(path);
            }
        } else if (path.endsWith(m_suffix) && !m_files.contains(path)) {
            add.append(path);
        }
    }
    watch(add);
    for (const QString &child : std::as_const(newFolders)) {
        addTree(child);
    }
}

void DirectoryWatcher::watch(const QStringList &paths) {
    if (paths.isEmpty()) {
        return;
    }
    const QStringList failed = m_watcher.addPaths(paths);
    for (const QString &path : paths) {
        if (!failed.contains(path)) {
            (QFileInfo(path).isDir() ? m_folders : m_files).insert(path);
        }
    }
    if (!failed.isEmpty()) {
        // Usually fs.inotify.max_user_watches: these paths stay unwatched until a change elsewhere retries them.
        qCWarning(watcherLog) << "cannot watch" << failed.size() << "paths, first:" << failed.first();
    }
}

void DirectoryWatcher::touched() {
    m_debounce.start();
}

} // namespace contribusage
