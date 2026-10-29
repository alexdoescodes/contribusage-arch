#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// ADR-050: gives every item under `target` the colors of a Plasma style chosen for this widget. Kirigami keeps a custom
// color on the item that sets it, so the labels, buttons and icons inside the popup would keep Plasma's colors; this
// sets them on each item, and on items created later (the blocks load after the popup). Empty `colors` resets them.
class StyleTint : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *target READ target WRITE setTarget NOTIFY changed)
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY changed)
    // An installed Plasma style's id: every Plasma image an item under `target` draws (`widgets/button`,
    // `widgets/tabbar`, …) is read from that style's folder instead of the desktop's style. Empty restores them.
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY changed)
    // The macOS glass: the popup window's own dialog background goes invisible, so the widget's see-through card lies
    // directly over the desktop's blur (the frame's mask still sets the blur region). False shows it again.
    Q_PROPERTY(bool hideFrame READ hideFrame WRITE setHideFrame NOTIFY changed)

public:
    using QObject::QObject;

    QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *target);
    QVariantMap colors() const { return m_colors; }
    void setColors(const QVariantMap &colors);
    QString style() const { return m_style; }
    void setStyle(const QString &style);
    bool hideFrame() const { return m_hideFrame; }
    void setHideFrame(bool hide);

Q_SIGNALS:
    void changed();

private:
    void apply(QQuickItem *item);
    void retarget(QObject *svg);

    QPointer<QQuickItem> m_target;
    QVariantMap m_colors;
    QString m_style;
    bool m_hideFrame = false;
};
