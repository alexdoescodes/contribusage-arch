#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Facts about the build for QML: the version in the popup footer.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    using QObject::QObject;
    QString version() const;
};
