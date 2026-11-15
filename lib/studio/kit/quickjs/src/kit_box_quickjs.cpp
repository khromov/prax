// The kinds of Studio Kit's boxes (pr/kit_box.h): `box`, `stack`, `layer`, `plane`, `spacer` and `divider`.
//
// No kind of this family shows its node's text or queues an event. Each but the spacer and the divider is a container:
// the nodes under it are declared in it, in the tree's order.
//
// A `box` (prKitBox) takes the fields of PrKitBoxStyle: `direction`, `justify`, `align` and `overflow`, each a keyword
// of its field's enumeration, `wrap`, a flag, `gap`, in px, and `padding`, one to four lengths in px. Its style's place
// is the node's placement. It also takes `disabled`, a flag, which disables the widgets under it.
//
// A `stack` (prKitStack) and a `layer` (prKitLayer) take a box's attributes. A layer covers the nearest stack or plane
// above it.
//
// A `plane` (prKitPlane) takes a box's attributes, and:
//
//   `transform`        sixteen numbers separated by spaces, which are the plane's matrix in the order that matrix3d()
//                      takes them, down each column in turn (css-transforms-2/#funcdef-matrix3d). Each may have an
//                      exponent, since a script writes the small terms of a rotation with one. Reset, the plane has no
//                      transform.
//   `perspective`      the distance to the eye in px, or 0 for none
//   `z-index`          a whole number, the plane's stack level, or 0 for none
//   `backface-hidden`  a flag
//
// A `spacer` (prKitSpacer) and a `divider` (prKitDivider) take no attributes of their own, and the nodes under them are
// declared in them as in a box, though neither is made to hold any.

#include "studio/kit/quickjs/src/kit_box_quickjs.h"

#include "pr/float.h"
#include "pr/float_convert.h"
#include "pr/geom.h"
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

// What a box, a stack or a layer is declared from. The style's place is the node's.
struct BoxData {
    PrKitBoxStyle style;
    bool disabled;
};

// What a plane is declared from. The place of the style's box is the node's.
struct PlaneData {
    PrKitPlaneStyle style;
    PrMatrix4 transform;            // used if `transformed` is true
    bool transformed;
    bool disabled;
};

// Reads sixteen numbers separated by spaces as a matrix, in the order that matrix3d() takes them
// (css-transforms-2/#funcdef-matrix3d). Returns false, and writes nothing, for any other text, or for a number beyond
// the finite range of a float.
bool matrixFrom(PrStringView text, PrMatrix4* out) {
    PrMatrix4 matrix{};
    uint32_t count = 0;
    size_t at = 0;
    while (at < text.size) {
        if (text.data[at] == ' ') {
            ++at;
            continue;
        }
        double number = 0;
        size_t taken = prF64ParsePrefix(PrStringView{text.data + at, text.size - at}, &number);
        at += taken;
        bool held = number >= -double(kPrF32Max) && number <= double(kPrF32Max);
        if (taken == 0 || count == 16 || !held || (at < text.size && text.data[at] != ' '))
            return false;
        matrix.m[count % 4][count / 4] = float(number);
        ++count;
    }
    if (count != 16)
        return false;
    *out = matrix;
    return true;
}

// Reads a plane's stack level: a whole number that an int32_t holds, or with `reset`, zero.
bool setLevel(PrStringView value, bool reset, int32_t* out) {
    double number = 0;
    if (!jsKitSetNumber(value, reset, &number) || !(number >= -2147483648.0 && number <= 2147483647.0))
        return false;
    int32_t level = int32_t(number);
    if (double(level) != number)
        return false;
    *out = level;
    return true;
}

bool boxSet(void* data, PrStringView name, PrStringView value, bool reset) {
    BoxData& box = *static_cast<BoxData*>(data);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &box.disabled);
    return jsKitBoxStyleSet(box.style, name, value, reset);
}

// Returns a node's box, and its style with the node's place.
PrKitBox boxOf(const JsKitRound& round, JsKitNodeIndex node, bool disabled) {
    return PrKitBox{.key = jsKitNodeKey(round, node), .disabled = disabled};
}
PrKitBoxStyle placed(const JsKitRound& round, JsKitNodeIndex node, PrKitBoxStyle style) {
    style.place = jsKitNodePlace(round.tree, node);
    return style;
}

PrLayoutNode boxDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const BoxData& box = jsKitData<BoxData>(round, node);
    return prKitBox(round.kit, parent, boxOf(round, node, box.disabled), placed(round, node, box.style));
}

PrLayoutNode stackDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const BoxData& box = jsKitData<BoxData>(round, node);
    return prKitStack(round.kit, parent, boxOf(round, node, box.disabled), placed(round, node, box.style));
}

PrLayoutNode layerDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const BoxData& box = jsKitData<BoxData>(round, node);
    return prKitLayer(round.kit, parent, boxOf(round, node, box.disabled), placed(round, node, box.style));
}

bool planeSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PlaneData& plane = *static_cast<PlaneData*>(data);
    if (name == "transform") {
        if (!reset && !matrixFrom(value, &plane.transform))
            return false;
        plane.transformed = !reset;
        return true;
    }
    if (name == "perspective")
        return jsKitSetLength(value, reset, &plane.style.perspective);
    if (name == "z-index")
        return setLevel(value, reset, &plane.style.zIndex);
    if (name == "backface-hidden")
        return jsKitSetFlag(value, reset, &plane.style.backfaceHidden);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &plane.disabled);
    return jsKitBoxStyleSet(plane.style.box, name, value, reset);
}

PrLayoutNode planeDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const PlaneData& plane = jsKitData<PlaneData>(round, node);
    PrKitPlaneStyle style = plane.style;
    style.box = placed(round, node, style.box);
    return prKitPlane(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .disabled = plane.disabled,
        .transform = plane.transformed ? &plane.transform : nullptr,
    }, style);
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
    {
        .name = "stack",
        .dataSize = sizeof(BoxData),
        .flags = JsKitKindFlag_PlacesItself,
        .set = boxSet,
        .declare = stackDeclare,
    },
    {
        .name = "layer",
        .dataSize = sizeof(BoxData),
        .flags = JsKitKindFlag_PlacesItself,
        .set = boxSet,
        .declare = layerDeclare,
    },
    {
        .name = "plane",
        .dataSize = sizeof(PlaneData),
        .flags = JsKitKindFlag_PlacesItself,
        .set = planeSet,
        .declare = planeDeclare,
    },
    {.name = "spacer", .declare = spacerDeclare},
    {.name = "divider", .declare = dividerDeclare},
};

} // namespace

Span<const JsKitKind> jsKitBoxKinds() {
    return kKinds;
}

bool jsKitBoxStyleSet(PrKitBoxStyle& style, PrStringView name, PrStringView value, bool reset) {
    if (name == "direction")
        return jsKitSetKeyword(kDirections, value, reset, &style.direction);
    if (name == "justify")
        return jsKitSetKeyword(kJustifications, value, reset, &style.justify);
    if (name == "align")
        return jsKitSetKeyword(jsKitAlignments(), value, reset, &style.align);
    if (name == "overflow")
        return jsKitSetKeyword(kOverflows, value, reset, &style.overflow);
    if (name == "wrap")
        return jsKitSetFlag(value, reset, &style.wrap);
    if (name == "gap")
        return jsKitSetLength(value, reset, &style.gap);
    if (name == "padding")
        return jsKitSetEdges(value, reset, &style.padding);
    return false;
}

} // namespace pr_impl
