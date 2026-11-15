// The kinds of Studio Kit's panels (pr/kit_panel.h), and the color that a swatch's attribute gives.

#pragma once

#include "pr/color.h"
#include "pr/kit.h"
#include "pr/span.h"
#include "pr/string_view.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// Returns the family's kinds.
Span<const JsKitKind> jsKitPanelKinds();

// A color that an attribute gives a widget whose content holds a PrColor: a color of the kit's palette, which follows
// the theme, or a color of its own. Zero is transparent black.
struct JsKitColorValue {
    JsKitColor named;               // JsKitColor_None if the color is `own`
    PrColor own;                    // in sRGB, with straight alpha, as the palette's colors are
};

// Reads a color, or with `reset` writes transparent black. A color is one of three forms: the name of a color of the
// palette (jsKitColors); `#` and three, four, six or eight hexadecimal digits (css-color-4/#hex-notation); or three or
// four numbers separated by spaces, which are the red, green, blue and alpha channels, where 1 is a channel's full
// value and alpha is 1 if it is left out. Returns false, and writes nothing, for any other text.
bool jsKitSetColorValue(PrStringView value, bool reset, JsKitColorValue* out);

// Returns the color that `value` stands for in the kit's theme.
PrColor jsKitColorFromValue(const PrKit& kit, const JsKitColorValue& value);

} // namespace pr_impl

PR_ASSUME_NONNULL_END
