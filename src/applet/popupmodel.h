#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// What the popup and the panel label show. Until the providers land (L-1 to L-3) it holds the macOS app's preview
// data, built through the same core functions the real data will use.
class PopupModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantList providers READ providers CONSTANT)
    Q_PROPERTY(QVariantMap heatmap READ heatmap CONSTANT)
    Q_PROPERTY(QVariantMap github READ github CONSTANT)
    Q_PROPERTY(QVariantMap label READ label CONSTANT)

public:
    explicit PopupModel(QObject *parent = nullptr);

    QVariantList providers() const { return m_providers; }
    QVariantMap heatmap() const { return m_heatmap; }
    QVariantMap github() const { return m_github; }
    QVariantMap label() const { return m_label; }

private:
    QVariantList m_providers;
    QVariantMap m_heatmap;
    QVariantMap m_github;
    QVariantMap m_label;
};
