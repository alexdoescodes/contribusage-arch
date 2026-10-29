#pragma once

#include <QString>

namespace contribusage {

// Where the widget keeps its data (port design §2, SPEC §10.7). A struct, not a seam: tests pass a temporary root
// (ADR-015).
struct AppPaths {
    QString root;

    // $XDG_DATA_HOME/contribusage, normally ~/.local/share/contribusage.
    static AppPaths live();
    // root/providers/<id>: everything a provider stores; "Delete data for this provider" removes it (US-12).
    QString providerFolder(const QString &providerId) const;
};

} // namespace contribusage
