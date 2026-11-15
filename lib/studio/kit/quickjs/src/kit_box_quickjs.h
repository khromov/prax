// The kinds of Studio Kit's boxes (pr/kit_box.h).

#pragma once

#include "pr/kit_box.h"
#include "pr/span.h"
#include "pr/string_view.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// Returns the family's kinds.
Span<const JsKitKind> jsKitBoxKinds();

// Sets or resets an attribute of how a box lays out its children: `direction`, `justify`, `align`, `overflow`, `wrap`,
// `gap` or `padding`. It leaves the style's place alone, which a kind takes from its node's placement as it declares
// the widget. Returns false, leaving the style as it was, for any other name or a value that does not suit the
// attribute. Every kind whose widget takes a PrKitBoxStyle reads these attributes with it.
bool jsKitBoxStyleSet(PrKitBoxStyle& style, PrStringView name, PrStringView value, bool reset);

} // namespace pr_impl

PR_ASSUME_NONNULL_END
