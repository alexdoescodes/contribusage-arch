#include "core/popoverlayout.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace contribusage {

namespace {

const QList<SectionKind> defaultSections{SectionKind::Limits, SectionKind::Activity, SectionKind::Insights};

// The elements `saved` names, in its order, then the others in theirs.
template <typename T> QList<T> ordered(const QList<T> &items, const QList<T> &saved) {
    QList<T> result;
    for (const T &item : saved) {
        if (items.contains(item) && !result.contains(item)) {
            result.append(item);
        }
    }
    for (const T &item : items) {
        if (!result.contains(item)) {
            result.append(item);
        }
    }
    return result;
}

template <typename T> void moveTo(QList<T> &items, const T &item, const T &target) {
    const qsizetype from = items.indexOf(item);
    const qsizetype to = items.indexOf(target);
    if (from < 0 || to < 0) {
        return;
    }
    items.move(from, to);
}

QJsonArray stringArray(const QStringList &list) {
    QJsonArray array;
    for (const QString &item : list) {
        array.append(item);
    }
    return array;
}

QStringList strings(const QJsonValue &value) {
    QStringList list;
    for (const QJsonValue &item : value.toArray()) {
        if (item.isString()) {
            list.append(item.toString());
        }
    }
    return list;
}

QStringList sortedList(const QSet<QString> &set) {
    QStringList list(set.cbegin(), set.cend());
    list.sort();
    return list;
}

} // namespace

QString sectionName(SectionKind kind) {
    switch (kind) {
    case SectionKind::Limits:
        return QStringLiteral("limits");
    case SectionKind::Activity:
        return QStringLiteral("activity");
    case SectionKind::Insights:
        return QStringLiteral("insights");
    }
    return {};
}

std::optional<SectionKind> sectionFromName(const QString &name) {
    for (const SectionKind kind : defaultSections) {
        if (sectionName(kind) == name) {
            return kind;
        }
    }
    return std::nullopt;
}

bool PopoverLayout::shows(SectionKind section, const QString &provider) const {
    return !hiddenSections.value(provider).contains(section);
}

void PopoverLayout::setShows(SectionKind section, const QString &provider, bool shown) {
    QSet<SectionKind> hidden = hiddenSections.value(provider);
    if (shown) {
        hidden.remove(section);
    } else {
        hidden.insert(section);
    }
    if (hidden.isEmpty()) {
        hiddenSections.remove(provider);
    } else {
        hiddenSections.insert(provider, hidden);
    }
}

QStringList PopoverLayout::arranged(const QStringList &registered) const {
    return ordered(QStringList(registered) << heatmapBlockId() << githubBlockId(), order);
}

QStringList PopoverLayout::blocks(const QStringList &registered, const QSet<QString> &on) const {
    QStringList shown;
    for (const QString &block : arranged(registered)) {
        if (!hiddenBlocks.contains(block) && isOn(block, on)) {
            shown.append(block);
        }
    }
    return shown;
}

void PopoverLayout::move(const QString &block, const QString &target, const QStringList &registered) {
    QStringList blocks = arranged(registered);
    moveTo(blocks, block, target);
    QStringList rest;
    for (const QString &key : std::as_const(order)) {
        if (!blocks.contains(key)) {
            rest.append(key);
        }
    }
    order = blocks + rest;
}

QList<SectionKind> PopoverLayout::sections(const QString &provider) const {
    return ordered(defaultSections, sectionOrder.value(provider));
}

QList<SectionKind> PopoverLayout::shownSections(const QString &provider) const {
    QList<SectionKind> shown;
    for (const SectionKind kind : sections(provider)) {
        if (shows(kind, provider)) {
            shown.append(kind);
        }
    }
    return shown;
}

void PopoverLayout::moveSection(SectionKind section, SectionKind target, const QString &provider) {
    QList<SectionKind> list = sections(provider);
    moveTo(list, section, target);
    sectionOrder.insert(provider, list);
}

bool PopoverLayout::isOn(const QString &block, const QSet<QString> &on) const {
    if (block == heatmapBlockId()) {
        for (const QString &source : on) {
            if (!outOfHeatmap.contains(source)) {
                return true;
            }
        }
        return false;
    }
    return on.contains(block);
}

QStringList PopoverLayout::heatmapLayers(const QStringList &sources, const QSet<QString> &on) const {
    QStringList layers;
    for (const QString &block : arranged(sources)) {
        if (block != heatmapBlockId() && on.contains(block) && !outOfHeatmap.contains(block)) {
            layers.append(block);
        }
    }
    return layers;
}

Demand PopoverLayout::demand(const QSet<QString> &on, const QSet<QString> &menuBar) const {
    const auto inShownHeatmap = [this](const QString &block) {
        return !hiddenBlocks.contains(heatmapBlockId()) && !outOfHeatmap.contains(block);
    };
    Demand demand;
    for (const QString &source : on) {
        if (source == githubBlockId()) {
            continue;
        }
        if ((!hiddenBlocks.contains(source) && shows(SectionKind::Activity, source)) || inShownHeatmap(source) ||
            menuBar.contains(source)) {
            demand.activity.insert(source);
        }
    }
    demand.github =
        on.contains(githubBlockId()) && (!hiddenBlocks.contains(githubBlockId()) || inShownHeatmap(githubBlockId()) ||
                                         menuBar.contains(githubBlockId()));
    return demand;
}

QJsonObject toJson(const PopoverLayout &layout) {
    QJsonObject hiddenSections;
    for (auto it = layout.hiddenSections.cbegin(); it != layout.hiddenSections.cend(); ++it) {
        QStringList names;
        for (const SectionKind kind : defaultSections) {
            if (it->contains(kind)) {
                names.append(sectionName(kind));
            }
        }
        hiddenSections.insert(it.key(), stringArray(names));
    }
    QJsonObject sectionOrder;
    for (auto it = layout.sectionOrder.cbegin(); it != layout.sectionOrder.cend(); ++it) {
        QStringList names;
        for (const SectionKind kind : *it) {
            names.append(sectionName(kind));
        }
        sectionOrder.insert(it.key(), stringArray(names));
    }
    return {{QStringLiteral("order"), stringArray(layout.order)},
            {QStringLiteral("hiddenBlocks"), stringArray(sortedList(layout.hiddenBlocks))},
            {QStringLiteral("hiddenSections"), hiddenSections},
            {QStringLiteral("sectionOrder"), sectionOrder},
            {QStringLiteral("outOfHeatmap"), stringArray(sortedList(layout.outOfHeatmap))},
            {QStringLiteral("heatmapStyle"), layout.heatmapStyle}};
}

PopoverLayout popoverLayoutFromJson(const QJsonObject &json) {
    PopoverLayout layout;
    layout.order = strings(json.value(QStringLiteral("order")));
    const QStringList hidden = strings(json.value(QStringLiteral("hiddenBlocks")));
    layout.hiddenBlocks = QSet<QString>(hidden.cbegin(), hidden.cend());
    const QJsonObject hiddenSections = json.value(QStringLiteral("hiddenSections")).toObject();
    for (auto it = hiddenSections.begin(); it != hiddenSections.end(); ++it) {
        QSet<SectionKind> kinds;
        for (const QString &name : strings(it.value())) {
            if (const auto kind = sectionFromName(name)) {
                kinds.insert(*kind);
            }
        }
        if (!kinds.isEmpty()) {
            layout.hiddenSections.insert(it.key(), kinds);
        }
    }
    const QJsonObject sectionOrder = json.value(QStringLiteral("sectionOrder")).toObject();
    for (auto it = sectionOrder.begin(); it != sectionOrder.end(); ++it) {
        QList<SectionKind> kinds;
        for (const QString &name : strings(it.value())) {
            if (const auto kind = sectionFromName(name)) {
                kinds.append(*kind);
            }
        }
        layout.sectionOrder.insert(it.key(), kinds);
    }
    const QStringList out = strings(json.value(QStringLiteral("outOfHeatmap")));
    layout.outOfHeatmap = QSet<QString>(out.cbegin(), out.cend());
    if (json.value(QStringLiteral("heatmapStyle")).toString() == QStringLiteral("stacked")) {
        layout.heatmapStyle = QStringLiteral("stacked");
    }
    return layout;
}

PopoverLayout popoverLayoutFromText(const QString &text) {
    const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
    return document.isObject() ? popoverLayoutFromJson(document.object()) : PopoverLayout();
}

QString popoverLayoutText(const PopoverLayout &layout) {
    return QString::fromUtf8(QJsonDocument(toJson(layout)).toJson(QJsonDocument::Compact));
}

} // namespace contribusage
