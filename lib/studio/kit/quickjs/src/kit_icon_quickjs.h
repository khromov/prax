// The kinds of Studio Kit's icons (pr/kit_icon.h).

#pragma once

#include "pr/span.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// Returns the family's kinds.
Span<const JsKitKind> jsKitIconKinds();

} // namespace pr_impl

PR_ASSUME_NONNULL_END
