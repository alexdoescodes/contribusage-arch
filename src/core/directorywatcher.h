#pragma once

#include <QFileSystemWatcher>
#include <QObject>
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
    void addTree(const QString &folder);
    void touched();

    QStringList m_roots;
    QString m_suffix;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

} // namespace contribusage
