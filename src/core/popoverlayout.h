#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace contribusage {

// FR-45: the reserved block keys besides the provider IDs.
inline QString heatmapBlockId() {
    return QStringLiteral("heatmap");
}
inline QString githubBlockId() {
    return QStringLiteral("github");
}

// A provider group's sections in their default order (FR-44).
enum class SectionKind { Limits, Activity, Insights };
QString sectionName(SectionKind kind);
std::optional<SectionKind> sectionFromName(const QString &name);

// What must run besides the limits of every provider that is on (FR-46).
struct Demand {
    QSet<QString> activity;
    bool github = false;
};

// SPEC §10.8, FR-44 to FR-49: the popup's arrangement. The default shows everything.
struct PopoverLayout {
    QStringList order; // saved order; may name unregistered providers, kept and ignored (FR-45)
    QSet<QString> hiddenBlocks;
    QMap<QString, QSet<SectionKind>> hiddenSections;
    QMap<QString, QList<SectionKind>> sectionOrder;    // missing sections follow in default order (ADR-034)
    QSet<QString> outOfHeatmap;                        // provider IDs or "github"
    QString heatmapStyle = QStringLiteral("combined"); // or "stacked" (FR-49)

    bool shows(SectionKind section, const QString &provider) const;
    // Showing every section again drops the provider's entry.
    void setShows(SectionKind section, const QString &provider, bool shown);
    // Visible blocks in order, given the registry order and the sources that are on (FR-44, FR-47).
    QStringList blocks(const QStringList &registered, const QSet<QString> &on) const;
    // Every registered block in order, hidden or off ones too: providers, heatmap, GitHub after the saved ones.
    QStringList arranged(const QStringList &registered) const;
    // Puts `block` in `target`'s place among the registered blocks; unregistered saved keys follow them.
    void move(const QString &block, const QString &target, const QStringList &registered);
    QList<SectionKind> sections(const QString &provider) const;
    QList<SectionKind> shownSections(const QString &provider) const;
    void moveSection(SectionKind section, SectionKind target, const QString &provider);
    // The heatmap is on while one of its layers is.
    bool isOn(const QString &block, const QSet<QString> &on) const;
    // The heatmap's layers in block order: sources that are on and in the heatmap, hidden blocks included.
    QStringList heatmapLayers(const QStringList &sources, const QSet<QString> &on) const;
    // FR-46: activity while its section or layer shows or the panel label draws it; GitHub likewise.
    Demand demand(const QSet<QString> &on, const QSet<QString> &menuBar) const;

    bool operator==(const PopoverLayout &) const = default;
};

QJsonObject toJson(const PopoverLayout &layout);
PopoverLayout popoverLayoutFromJson(const QJsonObject &json);
// The `popoverLayout` setting's text; empty or broken text is the default layout.
PopoverLayout popoverLayoutFromText(const QString &text);
QString popoverLayoutText(const PopoverLayout &layout);

} // namespace contribusage
