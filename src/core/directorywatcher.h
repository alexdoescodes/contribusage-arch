#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
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
    // Watches `folder` and what it holds that is not watched yet; descends only into folders that are new.
    void addTree(const QString &folder);
    void watch(const QStringList &paths);
    void touched();

    QStringList m_roots;
    QString m_suffix;
    QFileSystemWatcher m_watcher;
    QSet<QString> m_folders; // what m_watcher holds, kept here: its own lists are copies built on every call
    QSet<QString> m_files;
    QSet<QString> m_parents; // parents of roots that do not exist yet
    QTimer m_debounce;
};

} // namespace contribusage
