// The kinds of Studio Kit's docks and splits (pr/kit_dock.h).

#pragma once

#include "pr/span.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// Returns the family's kinds.
Span<const JsKitKind> jsKitDockKinds();

} // namespace pr_impl

PR_ASSUME_NONNULL_END
