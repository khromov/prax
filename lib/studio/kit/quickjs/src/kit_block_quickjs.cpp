// The kind that is lib/layout's own rather than the kit's: `block`, a block box with a border, padding and a
// background, which shows its node's text before the nodes under it.
//
// A block takes `padding`, `border`, the width of every side in px, `radius`, of every corner in px, `background`,
// `border-color` and `color`, each a color of the kit's palette by name, and `font`, a text role. Its text takes its
// parent's font and color unless the block names its own.

#include "studio/kit/quickjs/src/kit_block_quickjs.h"

#include "pr/color.h"
#include "pr/layout_frame.h"
#include "pr/layout_style.h"
#include "pr/layout_unit.h"
#include "pr/layout_value.h"
#include "pr/ui_records.h"

namespace pr_impl {

namespace {

struct BlockData {
    PrKitEdges padding;
    float border;
    float radius;
    JsKitColor background;
    JsKitColor borderColor;
    JsKitColor color;
    bool hasFont;
    PrKitTextRole font;
};

// The recipes of a block's record (pr/ui_records.h), each of which writes one group from the block's attributes. They
// read the kit's theme, so the kit owns what they make.

struct BlockBox {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Box;
    float border;
    PrKitEdges padding;
    void make(const PrKit&, PrLayoutBoxStyle& box) const {
        box.display = PrLayoutDisplay_Block;
        const float paddings[PrRectSide_Count] = {padding.top, padding.right, padding.bottom, padding.left};
        for (uint32_t side = 0; side < PrRectSide_Count; ++side) {
            box.border[side] = prLayoutUnitFromPx(border);
            box.padding[side] = prLayoutLengthPx(paddings[side]);
        }
    }
};

struct BlockPaint {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Decoration;
    float radius;
    JsKitColor background;
    JsKitColor borderColor;
    bool bordered;
    uint8_t _pad;
    void make(const PrKit& kit, PrLayoutDecorationStyle& decoration) const {
        if (background != JsKitColor_None)
            decoration.background = prColor16FromColor(jsKitColorFrom(kit, background));
        for (uint32_t side = 0; side < PrRectSide_Count; ++side) {
            if (borderColor != JsKitColor_None)
                decoration.borderColor[side] = prColor16FromColor(jsKitColorFrom(kit, borderColor));
            if (bordered)
                decoration.borderStyle[side] = PrDrawBorderStyle_Solid;
        }
        for (PrLayoutLength& corner : decoration.radii)
            corner = prLayoutLengthPx(radius);
    }
};

struct BlockType {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Font;
    PrKitTextRole role;
    void make(const PrKit& kit, PrLayoutFontStyle& font) const {
        font.font = kit.fonts[role];
    }
};

struct BlockInk {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Color;
    JsKitColor color;
    void make(const PrKit& kit, PrLayoutColorStyle& ink) const {
        ink.color = prColor16FromColor(jsKitColorFrom(kit, color));
    }
};

bool blockSet(void* data, PrStringView name, PrStringView value, bool reset) {
    BlockData& block = *static_cast<BlockData*>(data);
    if (name == "padding")
        return jsKitSetEdges(value, reset, &block.padding);
    if (name == "border")
        return jsKitSetLength(value, reset, &block.border);
    if (name == "radius")
        return jsKitSetLength(value, reset, &block.radius);
    if (name == "background")
        return jsKitSetKeyword(jsKitColors(), value, reset, &block.background);
    if (name == "border-color")
        return jsKitSetKeyword(jsKitColors(), value, reset, &block.borderColor);
    if (name == "color")
        return jsKitSetKeyword(jsKitColors(), value, reset, &block.color);
    if (name == "font") {
        if (!jsKitSetKeyword(jsKitRoles(), value, reset, &block.font))
            return false;
        block.hasFont = !reset;
        return true;
    }
    return false;
}

PrLayoutNode blockDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const BlockData& block = jsKitData<BlockData>(round, node);
    PrKit* kit = round.kit;
    PrLayoutStyle record = prLayoutStyleDefault();
    record.groups[PrLayoutGroup_Box] = uiGroup(kit->ui, *kit, BlockBox{block.border, block.padding});
    record.groups[PrLayoutGroup_Decoration] = uiGroup(kit->ui, *kit, BlockPaint{
        .radius = block.radius, .background = block.background, .borderColor = block.borderColor,
            .bordered = block.border > 0,
    });
    if (block.hasFont)
        record.groups[PrLayoutGroup_Font] = uiGroup(kit->ui, *kit, BlockType{block.font});
    if (block.color != JsKitColor_None)
        record.groups[PrLayoutGroup_Color] = uiGroup(kit->ui, *kit, BlockInk{block.color});
    PrLayoutStyleIndex style = prLayoutStyleIntern(kit->tree, &record);
    PrLayoutNode made = prLayoutTreeFrameElement(kit->tree, parent, jsKitNodeKey(round, node), style);
    PrStringView text = jsKitNodeText(round.tree, node);
    if (text.size > 0)
        prLayoutTreeFrameText(kit->tree, made, 0, text);
    return made;
}

constexpr JsKitKind kKinds[] = {
    {.name = "block", .dataSize = sizeof(BlockData), .set = blockSet, .declare = blockDeclare},
};

} // namespace

Span<const JsKitKind> jsKitBlockKinds() {
    return kKinds;
}

} // namespace pr_impl
