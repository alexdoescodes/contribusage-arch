#include "styletint.h"

#include "core/appearance.h"

#include <Kirigami/Platform/PlatformTheme>

#include <QStandardPaths>

using Kirigami::Platform::PlatformTheme;

void StyleTint::setTarget(QQuickItem *target) {
    if (m_target == target) {
        return;
    }
    m_target = target;
    if (m_target) {
        apply(m_target);
    }
    Q_EMIT changed();
}

void StyleTint::setColors(const QVariantMap &colors) {
    if (m_colors == colors) {
        return;
    }
    m_colors = colors;
    if (m_target) {
        apply(m_target);
    }
    Q_EMIT changed();
}

void StyleTint::setStyle(const QString &style) {
    if (m_style == style) {
        return;
    }
    m_style = style;
    if (m_target) {
        apply(m_target);
    }
    Q_EMIT changed();
}

void StyleTint::setHideFrame(bool hide) {
    if (m_hideFrame == hide) {
        return;
    }
    m_hideFrame = hide;
    if (m_target) {
        apply(m_target);
    }
    Q_EMIT changed();
}

// A KSvg item (FrameSvgItem) or Svg object: its theme-relative `imagePath` becomes the style's file; the relative path
// is kept to restore it.
void StyleTint::retarget(QObject *svg) {
    static const char *original = "_styleTintImagePath";
    QString relative = svg->property(original).toString();
    if (relative.isEmpty()) {
        relative = svg->property("imagePath").toString();
        if (relative.isEmpty() || relative.startsWith(u'/')) {
            return;
        }
        svg->setProperty(original, relative);
    }
    QString file;
    if (!m_style.isEmpty()) {
        file = contribusage::plasmaStyleFile(QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation),
                                             m_style, relative);
    }
    svg->setProperty("imagePath", file.isEmpty() ? relative : file);
}

void StyleTint::apply(QQuickItem *item) {
    // Without style colors an item that never had a theme needs none; one that had gets Plasma's colors back.
    auto *theme = qobject_cast<PlatformTheme *>(qmlAttachedPropertiesObject<PlatformTheme>(item, !m_colors.isEmpty()));
    if (theme) {
        const auto color = [this](const char *name) { return QColor(m_colors.value(QLatin1String(name)).toString()); };
        // An invalid color resets the custom one.
        theme->setCustomTextColor(color("textColor"));
        theme->setCustomBackgroundColor(color("backgroundColor"));
        theme->setCustomHighlightColor(color("highlightColor"));
        theme->setCustomNegativeTextColor(color("negativeTextColor"));
        theme->setCustomNeutralTextColor(color("neutralTextColor"));
        theme->setCustomPositiveTextColor(color("positiveTextColor"));
    }
    if (item->metaObject()->indexOfProperty("imagePath") >= 0 && item->metaObject()->indexOfProperty("prefix") >= 0) {
        retarget(item); // KSvg.FrameSvgItem
        if (item->property("_styleTintImagePath").toString() == QStringLiteral("dialogs/background")) {
            item->setOpacity(m_hideFrame ? 0 : 1);
        }
    } else if (auto *svg = item->property("svg").value<QObject *>();
               svg && svg->metaObject()->indexOfProperty("imagePath") >= 0) {
        retarget(svg); // KSvg.SvgItem
    }
    if (!item->property("_styleTintWatched").toBool()) {
        item->setProperty("_styleTintWatched", true);
        connect(item, &QQuickItem::childrenChanged, this, [this, item] {
            if (!m_colors.isEmpty() || !m_style.isEmpty() || m_hideFrame) {
                apply(item);
            }
        });
    }
    for (QQuickItem *child : item->childItems()) {
        apply(child);
    }
}
