// The kinds that are not the kit's: a block of lib/layout's own.

#pragma once

#include "pr/span.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// Returns the family's kinds.
Span<const JsKitKind> jsKitBlockKinds();

} // namespace pr_impl

PR_ASSUME_NONNULL_END
