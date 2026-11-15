// The kinds of Studio Kit's text (pr/kit_text.h): `label`.
//
// A label shows its node's text, and takes `role`, `tone`, `wrap`, `text-align` and `preserve`.

#include "studio/kit/quickjs/src/kit_text_quickjs.h"

#include "pr/kit_text.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kWraps[] = {
    {"auto", PrKitWrap_Auto},
    {"wrap", PrKitWrap_Wrap},
    {"nowrap", PrKitWrap_Nowrap},
    {"clip", PrKitWrap_Clip},
};

constexpr JsKitKeyword kTextAlignments[] = {
    {"start", PrKitTextAlign_Start},
    {"center", PrKitTextAlign_Center},
    {"end", PrKitTextAlign_End},
};

bool labelSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitTextStyle& style = *static_cast<PrKitTextStyle*>(data);
    if (name == "role")
        return jsKitSetKeyword(jsKitRoles(), value, reset, &style.role);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &style.tone);
    if (name == "wrap")
        return jsKitSetKeyword(kWraps, value, reset, &style.wrap);
    if (name == "text-align")
        return jsKitSetKeyword(kTextAlignments, value, reset, &style.align);
    if (name == "preserve")
        return jsKitSetFlag(value, reset, &style.preserve);
    return false;
}

PrLayoutNode labelDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitLabel label{.key = jsKitNodeKey(round, node), .text = jsKitNodeText(round.tree, node)};
    return prKitLabel(round.kit, parent, label, jsKitData<PrKitTextStyle>(round, node));
}

constexpr JsKitKind kKinds[] = {
    {.name = "label", .dataSize = sizeof(PrKitTextStyle), .set = labelSet, .declare = labelDeclare},
};

} // namespace

Span<const JsKitKind> jsKitTextKinds() {
    return kKinds;
}

} // namespace pr_impl
