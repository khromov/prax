// The kinds of Studio Kit's boxes (pr/kit_box.h): `box`, `spacer` and `divider`.
//
// A box takes `direction`, `justify`, `align`, `overflow`, `wrap`, `gap`, `padding` and `disabled`. A spacer and a
// divider take no attributes of their own.

#include "studio/kit/quickjs/src/kit_box_quickjs.h"

#include "pr/kit_box.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kDirections[] = {{"column", PrKitDirection_Column}, {"row", PrKitDirection_Row}};

constexpr JsKitKeyword kJustifications[] = {
    {"start", PrKitJustify_Start},
    {"end", PrKitJustify_End},
    {"center", PrKitJustify_Center},
    {"space-between", PrKitJustify_SpaceBetween},
    {"space-around", PrKitJustify_SpaceAround},
    {"space-evenly", PrKitJustify_SpaceEvenly},
};

constexpr JsKitKeyword kOverflows[] = {
    {"visible", PrKitOverflow_Visible},
    {"clip", PrKitOverflow_Clip},
    {"scroll", PrKitOverflow_Scroll},
    {"scroll-y", PrKitOverflow_ScrollY},
};

// A box's style, whose place is the node's, and what it holds.
struct BoxData {
    PrKitBoxStyle style;
    bool disabled;
};

bool boxSet(void* data, PrStringView name, PrStringView value, bool reset) {
    BoxData& box = *static_cast<BoxData*>(data);
    if (name == "direction")
        return jsKitSetKeyword(kDirections, value, reset, &box.style.direction);
    if (name == "justify")
        return jsKitSetKeyword(kJustifications, value, reset, &box.style.justify);
    if (name == "align")
        return jsKitSetKeyword(jsKitAlignments(), value, reset, &box.style.align);
    if (name == "overflow")
        return jsKitSetKeyword(kOverflows, value, reset, &box.style.overflow);
    if (name == "wrap")
        return jsKitSetFlag(value, reset, &box.style.wrap);
    if (name == "gap")
        return jsKitSetLength(value, reset, &box.style.gap);
    if (name == "padding")
        return jsKitSetEdges(value, reset, &box.style.padding);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &box.disabled);
    return false;
}

PrLayoutNode boxDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const BoxData& box = jsKitData<BoxData>(round, node);
    PrKitBoxStyle style = box.style;
    style.place = jsKitNodePlace(round.tree, node);
    return prKitBox(round.kit, parent, {.key = jsKitNodeKey(round, node), .disabled = box.disabled}, style);
}

PrLayoutNode spacerDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return prKitSpacer(round.kit, parent, {.key = jsKitNodeKey(round, node)});
}

PrLayoutNode dividerDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return prKitDivider(round.kit, parent, {.key = jsKitNodeKey(round, node)});
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "box",
        .dataSize = sizeof(BoxData),
        .flags = JsKitKindFlag_PlacesItself,
        .set = boxSet,
        .declare = boxDeclare,
    },
    {.name = "spacer", .declare = spacerDeclare},
    {.name = "divider", .declare = dividerDeclare},
};

} // namespace

Span<const JsKitKind> jsKitBoxKinds() {
    return kKinds;
}

} // namespace pr_impl
