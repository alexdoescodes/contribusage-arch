#include "core/apppaths.h"

#include <QStandardPaths>

namespace contribusage {

AppPaths AppPaths::live() {
    return {.root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                    QStringLiteral("/contribusage")};
}

QString AppPaths::providerFolder(const QString &providerId) const {
    return root + QStringLiteral("/providers/") + providerId;
}

} // namespace contribusage
