#pragma once

#include <QString>
#include <QVariantMap>

namespace contribusage {

// Port design §3, layer 2: token overrides from the `themeTokens` setting (a JSON object). Only known tokens of the
// right kind survive: colors as #rrggbb or #aarrggbb, opacities 0 to 1, sizes clamped to sane ranges.
QVariantMap themeTokensFromText(const QString &json);

// Port design §3: `look` (macOS, Plasma, Custom; default macOS), `density` (comfortable, compact) and `tokens` (the
// overrides, used by the Custom look only) from the plasmoid configuration.
QVariantMap appearanceFromMap(const QVariantMap &configuration);

} // namespace contribusage
