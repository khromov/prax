// JavaScript binding of Studio Kit: a tree of widgets that a script edits and each round declares.

#include "pr/kit_quickjs.h"

#include "pr/alloc.h"
#include "pr/assert.h"
#include "pr/color.h"
#include "pr/count_of.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_text.h"
#include "pr/layout_frame.h"
#include "pr/layout_style.h"
#include "pr/layout_unit.h"
#include "pr/layout_value.h"
#include "pr/quickjs_binding.h"
#include "pr/raw_array.h"
#include "pr/raw_string.h"
#include "pr/span.h"
#include "pr/ui_records.h"

namespace pr_impl {

namespace {

// What a node declares.
enum {
    JsKitKind_Root,         // nothing of its own: the node whose children the host declares under its own node
    JsKitKind_Box,
    JsKitKind_Label,
    JsKitKind_Button,
    JsKitKind_Badge,
    JsKitKind_Spacer,
    JsKitKind_Divider,
    JsKitKind_Block,        // a block of lib/layout's own, with a border, padding and a background, that holds text
};
typedef uint8_t JsKitKind;

// A color of the kit's palette, which a block names so that it follows the theme.
enum {
    JsKitColor_None,
    JsKitColor_Accent,
    JsKitColor_AccentWeak,
    JsKitColor_AccentText,
    JsKitColor_Success,
    JsKitColor_Warning,
    JsKitColor_Danger,
    JsKitColor_Bg0,
    JsKitColor_Bg1,
    JsKitColor_Bg2,
    JsKitColor_Bg3,
    JsKitColor_Bg4,
    JsKitColor_Line1,
    JsKitColor_Line2,
    JsKitColor_Line3,
    JsKitColor_Text1,
    JsKitColor_Text2,
    JsKitColor_Text3,
    JsKitColor_Text4,
    JsKitColor_TextInverse,
};
typedef uint8_t JsKitColor;

// The events that a script listens for on a node.
enum {
    JsKitEventFlag_Click = 1u << 0,     // a button was activated (prKitActivated)
};
typedef uint8_t JsKitEventFlags;

// A node's position in the tree's nodes. The root is at 0.
typedef uint32_t JsKitNodeIndex;
constexpr JsKitNodeIndex kJsKitNodeNone = 0xffffffffu;

// A keyword of an attribute, and the value that it stands for.
struct JsKitKeyword {
    PrStringView name;
    uint8_t value;
};

constexpr JsKitKeyword kKinds[] = {
    {"box", JsKitKind_Box},
    {"label", JsKitKind_Label},
    {"button", JsKitKind_Button},
    {"badge", JsKitKind_Badge},
    {"spacer", JsKitKind_Spacer},
    {"divider", JsKitKind_Divider},
    {"block", JsKitKind_Block},
};

constexpr JsKitKeyword kDirections[] = {{"column", PrKitDirection_Column}, {"row", PrKitDirection_Row}};

constexpr JsKitKeyword kJustifications[] = {
    {"start", PrKitJustify_Start},
    {"end", PrKitJustify_End},
    {"center", PrKitJustify_Center},
    {"space-between", PrKitJustify_SpaceBetween},
    {"space-around", PrKitJustify_SpaceAround},
    {"space-evenly", PrKitJustify_SpaceEvenly},
};

constexpr JsKitKeyword kAlignments[] = {
    {"auto", PrKitAlign_Auto},
    {"stretch", PrKitAlign_Stretch},
    {"start", PrKitAlign_Start},
    {"end", PrKitAlign_End},
    {"center", PrKitAlign_Center},
    {"baseline", PrKitAlign_Baseline},
};

constexpr JsKitKeyword kOverflows[] = {
    {"visible", PrKitOverflow_Visible},
    {"clip", PrKitOverflow_Clip},
    {"scroll", PrKitOverflow_Scroll},
    {"scroll-y", PrKitOverflow_ScrollY},
};

constexpr JsKitKeyword kFlexes[] = {
    {"own", PrKitFlex_Own},
    {"auto", PrKitFlex_Auto},
    {"fixed", PrKitFlex_Fixed},
    {"fill", PrKitFlex_Fill},
    {"grow", PrKitFlex_Grow},
};

constexpr JsKitKeyword kRoles[] = {
    {"body", PrKitTextRole_Body},
    {"body-small", PrKitTextRole_BodySmall},
    {"strong", PrKitTextRole_Strong},
    {"body-italic", PrKitTextRole_BodyItalic},
    {"heading", PrKitTextRole_Heading},
    {"title", PrKitTextRole_Title},
    {"display", PrKitTextRole_Display},
    {"caption", PrKitTextRole_Caption},
    {"meta", PrKitTextRole_Meta},
    {"mono", PrKitTextRole_Mono},
    {"mono-strong", PrKitTextRole_MonoStrong},
    {"mono-num", PrKitTextRole_MonoNum},
};

constexpr JsKitKeyword kTones[] = {
    {"auto", PrKitTone_Auto},
    {"primary", PrKitTone_Primary},
    {"secondary", PrKitTone_Secondary},
    {"muted", PrKitTone_Muted},
    {"faint", PrKitTone_Faint},
    {"accent", PrKitTone_Accent},
    {"success", PrKitTone_Success},
    {"warning", PrKitTone_Warning},
    {"danger", PrKitTone_Danger},
    {"inverse", PrKitTone_Inverse},
};

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

constexpr JsKitKeyword kButtonVariants[] = {
    {"default", PrKitButtonVariant_Default},
    {"primary", PrKitButtonVariant_Primary},
    {"ghost", PrKitButtonVariant_Ghost},
    {"danger", PrKitButtonVariant_Danger},
};

constexpr JsKitKeyword kControlSizes[] = {
    {"md", PrKitControlSize_Md},
    {"sm", PrKitControlSize_Sm},
    {"xs", PrKitControlSize_Xs},
    {"lg", PrKitControlSize_Lg},
};

constexpr JsKitKeyword kBadgeVariants[] = {
    {"neutral", PrKitBadgeVariant_Neutral},
    {"accent", PrKitBadgeVariant_Accent},
    {"success", PrKitBadgeVariant_Success},
    {"warning", PrKitBadgeVariant_Warning},
    {"danger", PrKitBadgeVariant_Danger},
};

constexpr JsKitKeyword kColors[] = {
    {"accent", JsKitColor_Accent},
    {"accent-weak", JsKitColor_AccentWeak},
    {"accent-text", JsKitColor_AccentText},
    {"success", JsKitColor_Success},
    {"warning", JsKitColor_Warning},
    {"danger", JsKitColor_Danger},
    {"bg0", JsKitColor_Bg0},
    {"bg1", JsKitColor_Bg1},
    {"bg2", JsKitColor_Bg2},
    {"bg3", JsKitColor_Bg3},
    {"bg4", JsKitColor_Bg4},
    {"line1", JsKitColor_Line1},
    {"line2", JsKitColor_Line2},
    {"line3", JsKitColor_Line3},
    {"text1", JsKitColor_Text1},
    {"text2", JsKitColor_Text2},
    {"text3", JsKitColor_Text3},
    {"text4", JsKitColor_Text4},
    {"text-inverse", JsKitColor_TextInverse},
};

constexpr JsKitKeyword kEvents[] = {{"click", JsKitEventFlag_Click}};

// What a block's record is made from, beyond its padding, which is its box style's.
struct JsKitBlockStyle {
    float border;                   // the width of every side, in px
    float radius;                   // of every corner, in px
    JsKitColor background;
    JsKitColor borderColor;
    JsKitColor color;               // of its text, or None for its parent's
    bool hasFont;                   // false takes its parent's font
    PrKitTextRole font;
};

// One node, and every attribute that a node of any kind has. A kind reads the attributes that it takes, and each of the
// others keeps its zero.
struct JsKitNode {
    JsKitKind kind;
    bool used;                      // false for a freed node, which a later create reuses
    bool disabled;
    bool selected;
    bool placed;                    // `place` was set, so a widget that is not a box is placed with it
    JsKitEventFlags listens;

    // The node's place in the tree, each kJsKitNodeNone if there is none.
    JsKitNodeIndex parent;
    JsKitNodeIndex firstChild;
    JsKitNodeIndex lastChild;
    JsKitNodeIndex previous;
    JsKitNodeIndex next;

    PrRawString text;
    PrKitBoxStyle box;
    PrKitTextStyle type;
    PrKitButtonStyle button;
    PrKitBadgeStyle badge;
    PrKitPlace place;
    JsKitBlockStyle block;
};

// An event that a round queued for the script.
struct JsKitEvent {
    JsKitNodeIndex node;
    JsKitEventFlags type;           // one flag
};

// The nodes that a script made, and the events of the last round that it has yet to hear of.
struct JsKitTree {
    RawArray<JsKitNode> nodes;      // by JsKitNodeIndex
    RawArray<JsKitNodeIndex> free;  // the freed nodes
    RawArray<JsKitEvent> events;
    JSContext* PR_NULLABLE ctx;     // the realm that the tree is bound to
};

JSClassID kitTreeClass;

// ---------------------------------------------------------------------------------------------------------------------
// Values

// Finds the keyword `text` among `keywords`. Returns false if it is not one of them.
bool keywordFrom(Span<const JsKitKeyword> keywords, PrStringView text, uint8_t* out) {
    for (const JsKitKeyword& keyword : keywords) {
        if (keyword.name == text) {
            *out = keyword.value;
            return true;
        }
    }
    return false;
}

// Returns the name of `value` among `keywords`, or the empty string if none stands for it.
PrStringView keywordName(Span<const JsKitKeyword> keywords, uint8_t value) {
    for (const JsKitKeyword& keyword : keywords) {
        if (keyword.value == value)
            return keyword.name;
    }
    return "";
}

// Reads a decimal number that is the whole of `text`: an optional sign, digits, and an optional fraction. Returns false
// for anything else.
bool numberFrom(PrStringView text, float* out) {
    size_t at = 0;
    bool negative = false;
    if (at < text.size && (text.data[at] == '-' || text.data[at] == '+'))
        negative = text.data[at++] == '-';
    double mantissa = 0;
    double divisor = 1;
    bool digits = false;
    bool fraction = false;
    for (; at < text.size; ++at) {
        char c = text.data[at];
        if (c == '.' && !fraction) {
            fraction = true;
            continue;
        }
        if (c < '0' || c > '9')
            return false;
        mantissa = mantissa * 10 + (c - '0');
        if (fraction)
            divisor *= 10;
        digits = true;
    }
    if (!digits)
        return false;
    double value = mantissa / divisor;
    *out = float(negative ? -value : value);
    return true;
}

// Reads a number that is zero or more, as a length is.
bool lengthFrom(PrStringView text, float* out) {
    float value = 0;
    if (!numberFrom(text, &value) || value < 0)
        return false;
    *out = value;
    return true;
}

// Reads a boolean attribute: present with no value, or `true`, is true, and `false` is false.
bool flagFrom(PrStringView text, bool* out) {
    if (text == "" || text == "true")
        *out = true;
    else if (text == "false")
        *out = false;
    else
        return false;
    return true;
}

// Reads one to four lengths separated by spaces, which give the edges as CSS's shorthands do: every edge, the vertical
// and the horizontal edges, the top, the horizontal edges and the bottom, or the four edges from the top clockwise.
bool edgesFrom(PrStringView text, PrKitEdges* out) {
    float values[4] = {};
    uint32_t count = 0;
    size_t at = 0;
    while (at < text.size) {
        if (text.data[at] == ' ') {
            ++at;
            continue;
        }
        size_t end = at;
        while (end < text.size && text.data[end] != ' ')
            ++end;
        if (count == 4 || !lengthFrom(PrStringView{text.data + at, end - at}, &values[count]))
            return false;
        ++count;
        at = end;
    }
    switch (count) {
        case 1:
            *out = PrKitEdges{values[0], values[0], values[0], values[0]};
            return true;
        case 2:
            *out = PrKitEdges{values[0], values[1], values[0], values[1]};
            return true;
        case 3:
            *out = PrKitEdges{values[0], values[1], values[2], values[1]};
            return true;
        case 4:
            *out = PrKitEdges{values[0], values[1], values[2], values[3]};
            return true;
        default:
            return false;
    }
}

// Reads a size: `auto`, `fill`, a length in px, or a percentage of the parent's size.
bool sizeFrom(PrStringView text, PrKitSize* out) {
    float value = 0;
    if (text == "auto") {
        *out = PrKitSize{};
    } else if (text == "fill") {
        *out = prKitFill();
    } else if (text.size > 1 && text.data[text.size - 1] == '%') {
        if (!lengthFrom(PrStringView{text.data, text.size - 1}, &value))
            return false;
        *out = prKitPercent(value);
    } else {
        if (!lengthFrom(text, &value))
            return false;
        *out = prKitPx(value);
    }
    return true;
}

// Returns a palette color of the kit's theme, or transparent black for None.
PrColor colorOf(const PrKit& kit, JsKitColor color) {
    const PrKitPalette& palette = kit.theme.palette;
    switch (color) {
        case JsKitColor_Accent:
            return palette.accent;
        case JsKitColor_AccentWeak:
            return palette.accentWeak;
        case JsKitColor_AccentText:
            return palette.accentText;
        case JsKitColor_Success:
            return palette.success;
        case JsKitColor_Warning:
            return palette.warning;
        case JsKitColor_Danger:
            return palette.danger;
        case JsKitColor_Bg0:
            return palette.bg0;
        case JsKitColor_Bg1:
            return palette.bg1;
        case JsKitColor_Bg2:
            return palette.bg2;
        case JsKitColor_Bg3:
            return palette.bg3;
        case JsKitColor_Bg4:
            return palette.bg4;
        case JsKitColor_Line1:
            return palette.line1;
        case JsKitColor_Line2:
            return palette.line2;
        case JsKitColor_Line3:
            return palette.line3;
        case JsKitColor_Text1:
            return palette.text1;
        case JsKitColor_Text2:
            return palette.text2;
        case JsKitColor_Text3:
            return palette.text3;
        case JsKitColor_Text4:
            return palette.text4;
        case JsKitColor_TextInverse:
            return palette.textInverse;
        default:
            return PrColor{};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Attributes

// Sets a keyword attribute from `value`, or resets it to its zero. Returns false if `value` is none of `keywords`.
bool setKeyword(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out) {
    if (reset) {
        *out = 0;
        return true;
    }
    return keywordFrom(keywords, value, out);
}

bool setLength(PrStringView value, bool reset, float* out) {
    if (reset) {
        *out = 0;
        return true;
    }
    return lengthFrom(value, out);
}

bool setFlag(PrStringView value, bool reset, bool* out) {
    if (reset) {
        *out = false;
        return true;
    }
    return flagFrom(value, out);
}

bool setEdges(PrStringView value, bool reset, PrKitEdges* out) {
    if (reset) {
        *out = PrKitEdges{};
        return true;
    }
    return edgesFrom(value, out);
}

bool setSize(PrStringView value, bool reset, PrKitSize* out) {
    if (reset) {
        *out = PrKitSize{};
        return true;
    }
    return sizeFrom(value, out);
}

// Sets an attribute of how any widget sits in its parent. Returns false if `name` is not one, or `value` does not suit
// it.
bool setPlacement(PrKitPlace& place, PrStringView name, PrStringView value, bool reset) {
    if (name == "width")
        return setSize(value, reset, &place.width);
    if (name == "height")
        return setSize(value, reset, &place.height);
    if (name == "min-width")
        return setSize(value, reset, &place.minWidth);
    if (name == "min-height")
        return setSize(value, reset, &place.minHeight);
    if (name == "max-width")
        return setSize(value, reset, &place.maxWidth);
    if (name == "max-height")
        return setSize(value, reset, &place.maxHeight);
    if (name == "margin")
        return setEdges(value, reset, &place.margin);
    if (name == "flex")
        return setKeyword(kFlexes, value, reset, &place.flex);
    if (name == "align-self")
        return setKeyword(kAlignments, value, reset, &place.align);
    if (name == "weight")
        return setLength(value, reset, &place.weight);
    return false;
}

// Sets the attribute `name` of a node from `value`, or resets it to its default. Returns false, and changes nothing, if
// the node's kind has no such attribute or `value` does not suit it.
bool setAttribute(JsKitNode& node, PrStringView name, PrStringView value, bool reset) {
    if (node.kind == JsKitKind_Root)
        return false;
    if (setPlacement(node.place, name, value, reset)) {
        node.placed = true;
        return true;
    }
    switch (node.kind) {
        case JsKitKind_Box:
            if (name == "direction")
                return setKeyword(kDirections, value, reset, &node.box.direction);
            if (name == "justify")
                return setKeyword(kJustifications, value, reset, &node.box.justify);
            if (name == "align")
                return setKeyword(kAlignments, value, reset, &node.box.align);
            if (name == "overflow")
                return setKeyword(kOverflows, value, reset, &node.box.overflow);
            if (name == "wrap")
                return setFlag(value, reset, &node.box.wrap);
            if (name == "gap")
                return setLength(value, reset, &node.box.gap);
            if (name == "padding")
                return setEdges(value, reset, &node.box.padding);
            if (name == "disabled")
                return setFlag(value, reset, &node.disabled);
            return false;
        case JsKitKind_Label:
            if (name == "role")
                return setKeyword(kRoles, value, reset, &node.type.role);
            if (name == "tone")
                return setKeyword(kTones, value, reset, &node.type.tone);
            if (name == "wrap")
                return setKeyword(kWraps, value, reset, &node.type.wrap);
            if (name == "text-align")
                return setKeyword(kTextAlignments, value, reset, &node.type.align);
            if (name == "preserve")
                return setFlag(value, reset, &node.type.preserve);
            return false;
        case JsKitKind_Button:
            if (name == "variant")
                return setKeyword(kButtonVariants, value, reset, &node.button.variant);
            if (name == "size")
                return setKeyword(kControlSizes, value, reset, &node.button.size);
            if (name == "disabled")
                return setFlag(value, reset, &node.disabled);
            if (name == "selected")
                return setFlag(value, reset, &node.selected);
            return false;
        case JsKitKind_Badge:
            if (name == "variant")
                return setKeyword(kBadgeVariants, value, reset, &node.badge.variant);
            return false;
        case JsKitKind_Block:
            if (name == "padding")
                return setEdges(value, reset, &node.box.padding);
            if (name == "border")
                return setLength(value, reset, &node.block.border);
            if (name == "radius")
                return setLength(value, reset, &node.block.radius);
            if (name == "background")
                return setKeyword(kColors, value, reset, &node.block.background);
            if (name == "border-color")
                return setKeyword(kColors, value, reset, &node.block.borderColor);
            if (name == "color")
                return setKeyword(kColors, value, reset, &node.block.color);
            if (name == "font") {
                node.block.hasFont = !reset;
                return setKeyword(kRoles, value, reset, &node.block.font);
            }
            return false;
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The tree

// Returns a detached node of `kind`, in a freed node's place if there is one. A freed node is not reused while events
// are queued, so that an event never reaches a node made after it.
JsKitNodeIndex nodeCreate(JsKitTree* tree, JsKitKind kind) {
    JsKitNodeIndex index = tree->nodes.count;
    if (tree->free.count > 0 && tree->events.count == 0) {
        index = tree->free.last();
        tree->free.pop();
    } else {
        tree->nodes.emplace();
    }
    JsKitNode node{};
    node.kind = kind;
    node.used = true;
    node.parent = kJsKitNodeNone;
    node.firstChild = kJsKitNodeNone;
    node.lastChild = kJsKitNodeNone;
    node.previous = kJsKitNodeNone;
    node.next = kJsKitNodeNone;
    tree->nodes.data[index] = node;
    return index;
}

// Detaches a node from its parent, if it has one.
void nodeUnlink(JsKitTree* tree, JsKitNodeIndex index) {
    JsKitNode& node = tree->nodes.data[index];
    if (node.parent == kJsKitNodeNone)
        return;
    JsKitNode& parent = tree->nodes.data[node.parent];
    if (node.previous != kJsKitNodeNone)
        tree->nodes.data[node.previous].next = node.next;
    else
        parent.firstChild = node.next;
    if (node.next != kJsKitNodeNone)
        tree->nodes.data[node.next].previous = node.previous;
    else
        parent.lastChild = node.previous;
    node.parent = kJsKitNodeNone;
    node.previous = kJsKitNodeNone;
    node.next = kJsKitNodeNone;
}

// Puts a detached node under `parentIndex`, before its child `before`, or last if `before` is kJsKitNodeNone.
void nodeLink(JsKitTree* tree, JsKitNodeIndex parentIndex, JsKitNodeIndex index, JsKitNodeIndex before) {
    JsKitNode& parent = tree->nodes.data[parentIndex];
    JsKitNode& node = tree->nodes.data[index];
    node.parent = parentIndex;
    node.next = before;
    if (before != kJsKitNodeNone) {
        node.previous = tree->nodes.data[before].previous;
        tree->nodes.data[before].previous = index;
    } else {
        node.previous = parent.lastChild;
        parent.lastChild = index;
    }
    if (node.previous != kJsKitNodeNone)
        tree->nodes.data[node.previous].next = index;
    else
        parent.firstChild = index;
}

// Frees a detached node and the nodes under it.
void nodeFree(JsKitTree* tree, JsKitNodeIndex index) {
    JsKitNodeIndex child = tree->nodes.data[index].firstChild;
    while (child != kJsKitNodeNone) {
        JsKitNodeIndex next = tree->nodes.data[child].next;
        nodeFree(tree, child);
        child = next;
    }
    JsKitNode& node = tree->nodes.data[index];
    prRawStringReset(&node.text);
    node = JsKitNode{};
    tree->free.push(index);
}

// Returns true if `index` is `ancestor` or a node under it.
bool nodeIsWithin(const JsKitTree* tree, JsKitNodeIndex index, JsKitNodeIndex ancestor) {
    for (; index != kJsKitNodeNone; index = tree->nodes.data[index].parent) {
        if (index == ancestor)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rounds

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
            decoration.background = prColor16FromColor(colorOf(kit, background));
        for (uint32_t side = 0; side < PrRectSide_Count; ++side) {
            if (borderColor != JsKitColor_None)
                decoration.borderColor[side] = prColor16FromColor(colorOf(kit, borderColor));
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
        ink.color = prColor16FromColor(colorOf(kit, color));
    }
};

// Returns the record of a block. Its font and its color are its parent's unless the block names its own.
PrLayoutStyleIndex blockStyle(PrKit* kit, const JsKitNode& node) {
    PrLayoutStyle record = prLayoutStyleDefault();
    record.groups[PrLayoutGroup_Box] = uiGroup(kit->ui, *kit, BlockBox{node.block.border, node.box.padding});
    record.groups[PrLayoutGroup_Decoration] = uiGroup(kit->ui, *kit, BlockPaint{
        .radius = node.block.radius, .background = node.block.background, .borderColor = node.block.borderColor,
            .bordered = node.block.border > 0,
    });
    if (node.block.hasFont)
        record.groups[PrLayoutGroup_Font] = uiGroup(kit->ui, *kit, BlockType{node.block.font});
    if (node.block.color != JsKitColor_None)
        record.groups[PrLayoutGroup_Color] = uiGroup(kit->ui, *kit, BlockInk{node.block.color});
    return prLayoutStyleIntern(kit->tree, &record);
}

// Declares the children of a node under `parent`, in order.
void declareChildren(JsKitTree* tree, PrKit* kit, PrLayoutNode base, PrLayoutNode parent, JsKitNodeIndex index);

// Declares a node's widget under `parent`, then the nodes under it, and queues the widget's events that the script
// listens for. The node's key is made from its index under `base`, the host's node, so that the widget keeps its state
// wherever the script moves the node.
void declare(JsKitTree* tree, PrKit* kit, PrLayoutNode base, PrLayoutNode parent, JsKitNodeIndex index) {
    const JsKitNode& node = tree->nodes.data[index];
    PrUiKey key = prUiKeyIndex(kit->ui, base, index);
    PrStringView text = prRawStringView(&node.text);
    PrLayoutNode made = kPrLayoutNodeNone;
    switch (node.kind) {
        case JsKitKind_Box: {
            PrKitBoxStyle style = node.box;
            style.place = node.place;
            made = prKitBox(kit, parent, {.key = key, .disabled = node.disabled}, style);
            declareChildren(tree, kit, base, made, index);
            return;
        }
        case JsKitKind_Label:
            made = prKitLabel(kit, parent, {.key = key, .text = text}, node.type);
            break;
        case JsKitKind_Button:
            made = prKitButton(kit, parent, {
                .key = key,
                .text = text,
                .selected = node.selected,
                .disabled = node.disabled,
            }, node.button);
            break;
        case JsKitKind_Badge:
            made = prKitBadge(kit, parent, {.key = key, .text = text}, node.badge);
            break;
        case JsKitKind_Spacer:
            made = prKitSpacer(kit, parent, {.key = key});
            break;
        case JsKitKind_Divider:
            made = prKitDivider(kit, parent, {.key = key});
            break;
        case JsKitKind_Block:
            made = prLayoutTreeFrameElement(kit->tree, parent, key, blockStyle(kit, node));
            if (text.size > 0)
                prLayoutTreeFrameText(kit->tree, made, 0, text);
            declareChildren(tree, kit, base, made, index);
            break;
        default:
            return;
    }
    if (node.placed)
        prKitPlace(kit, made, node.place);
    if ((node.listens & JsKitEventFlag_Click) != 0 && prKitActivated(kit, made))
        tree->events.push({index, JsKitEventFlag_Click});
}

void declareChildren(JsKitTree* tree, PrKit* kit, PrLayoutNode base, PrLayoutNode parent, JsKitNodeIndex index) {
    for (JsKitNodeIndex child = tree->nodes.data[index].firstChild; child != kJsKitNodeNone;
            child = tree->nodes.data[child].next)
        declare(tree, kit, base, parent, child);
}

// ---------------------------------------------------------------------------------------------------------------------
// kitTree

JsKitTree* PR_NULLABLE treeOf(JSContext* ctx, JSValueConst treeObject) {
    return static_cast<JsKitTree*>(JS_GetOpaque2(ctx, treeObject, kitTreeClass));
}

// Reads `value` as a node of the tree. Returns false, with a TypeError pending, if it names none.
bool readNode(JSContext* ctx, const JsKitTree* tree, JSValueConst value, JsKitNodeIndex* out) {
    uint32_t index = 0;
    if (JS_ToUint32(ctx, &index, value) < 0)
        return false;
    if (index >= tree->nodes.count || !tree->nodes.data[index].used) {
        JS_ThrowTypeError(ctx, "kitTree: there is no node %u", index);
        return false;
    }
    *out = index;
    return true;
}

// Reads `value` as a node that a script made, which the root is not.
bool readOwnNode(JSContext* ctx, const JsKitTree* tree, JSValueConst value, JsKitNodeIndex* out) {
    if (!readNode(ctx, tree, value, out))
        return false;
    if (*out == 0) {
        JS_ThrowTypeError(ctx, "kitTree: the root stays as it is");
        return false;
    }
    return true;
}

JSValue createMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    PrStringView name = {};
    if (tree == nullptr || argc < 1 || !jsReadString(ctx, argv[0], &name))
        return JS_EXCEPTION;
    JsKitKind kind = JsKitKind_Root;
    if (!keywordFrom(kKinds, name, &kind))
        return JS_ThrowTypeError(ctx, "kitTree: there is no kind of node named %.*s", int(name.size), name.data);
    return JS_NewUint32(ctx, nodeCreate(tree, kind));
}

JSValue destroyMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex index = 0;
    if (tree == nullptr || argc < 1 || !readOwnNode(ctx, tree, argv[0], &index))
        return JS_EXCEPTION;
    nodeUnlink(tree, index);
    nodeFree(tree, index);
    return JS_UNDEFINED;
}

JSValue insertMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex parent = 0;
    JsKitNodeIndex index = 0;
    JsKitNodeIndex before = kJsKitNodeNone;
    if (tree == nullptr || argc < 2 || !readNode(ctx, tree, argv[0], &parent) || !readOwnNode(ctx, tree, argv[1], &index))
        return JS_EXCEPTION;
    uint32_t given = 0;
    if (argc > 2 && JS_ToUint32(ctx, &given, argv[2]) < 0)
        return JS_EXCEPTION;
    if (given != 0 && !readNode(ctx, tree, argv[2], &before))
        return JS_EXCEPTION;
    if (nodeIsWithin(tree, parent, index))
        return JS_ThrowTypeError(ctx, "kitTree: node %u cannot go under itself", index);
    if (before != kJsKitNodeNone && (before == index || tree->nodes.data[before].parent != parent))
        return JS_ThrowTypeError(ctx, "kitTree: node %u is not another child of node %u", before, parent);
    nodeUnlink(tree, index);
    nodeLink(tree, parent, index, before);
    return JS_UNDEFINED;
}

JSValue removeMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex index = 0;
    if (tree == nullptr || argc < 1 || !readOwnNode(ctx, tree, argv[0], &index))
        return JS_EXCEPTION;
    nodeUnlink(tree, index);
    return JS_UNDEFINED;
}

JSValue setMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex index = 0;
    PrStringView name = {};
    PrStringView value = {};
    if (tree == nullptr || argc < 2 || !readNode(ctx, tree, argv[0], &index) || !jsReadString(ctx, argv[1], &name))
        return JS_EXCEPTION;
    bool reset = argc < 3 || JS_IsNull(argv[2]) || JS_IsUndefined(argv[2]);
    if (!reset && !jsReadString(ctx, argv[2], &value))
        return JS_EXCEPTION;
    // An attribute changes only if its value suits it, so a value that does not leaves the node as it was.
    JsKitNode changed = tree->nodes.data[index];
    if (!setAttribute(changed, name, value, reset))
        return JS_FALSE;
    tree->nodes.data[index] = changed;
    return JS_TRUE;
}

JSValue setTextMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex index = 0;
    PrStringView text = {};
    if (tree == nullptr || argc < 2 || !readOwnNode(ctx, tree, argv[0], &index) || !jsReadString(ctx, argv[1], &text))
        return JS_EXCEPTION;
    prRawStringReplace(&tree->nodes.data[index].text, text);
    return JS_UNDEFINED;
}

JSValue listenMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex index = 0;
    PrStringView name = {};
    if (tree == nullptr || argc < 3 || !readOwnNode(ctx, tree, argv[0], &index) || !jsReadString(ctx, argv[1], &name))
        return JS_EXCEPTION;
    JsKitEventFlags type = 0;
    JsKitNode& node = tree->nodes.data[index];
    if (!keywordFrom(kEvents, name, &type) || node.kind != JsKitKind_Button)
        return JS_FALSE;
    if (JS_ToBool(ctx, argv[2]) > 0)
        node.listens |= type;
    else
        node.listens &= JsKitEventFlags(~type);
    return JS_TRUE;
}

constexpr JSCFunctionListEntry kTreeFunctions[] = {
    JS_CFUNC_DEF("create", 1, JsCallScoped<createMethod>::call),
    JS_CFUNC_DEF("destroy", 1, JsCallScoped<destroyMethod>::call),
    JS_CFUNC_DEF("insert", 3, JsCallScoped<insertMethod>::call),
    JS_CFUNC_DEF("remove", 1, JsCallScoped<removeMethod>::call),
    JS_CFUNC_DEF("set", 3, JsCallScoped<setMethod>::call),
    JS_CFUNC_DEF("setText", 2, JsCallScoped<setTextMethod>::call),
    JS_CFUNC_DEF("listen", 3, JsCallScoped<listenMethod>::call),
};

// Hands an exception that the script's listener threw to the realm's console, which is where a host looks for a
// script's errors.
void reportException(JSContext* ctx) {
    JSValue exception = JS_GetException(ctx);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_GetPropertyStr(ctx, global, "console");
    JSValue error = JS_GetPropertyStr(ctx, console, "error");
    if (JS_IsFunction(ctx, error))
        JS_FreeValue(ctx, JS_Call(ctx, error, console, 1, &exception));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, console);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, exception);
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

// C opaque types
struct PrJsKitTree : JsKitTree {};

PrJsKitTree* prJsKitTreeCreate(void) {
    PrJsKitTree* tree = allocZeroed<PrJsKitTree>();
    nodeCreate(tree, JsKitKind_Root);
    return tree;
}

void prJsKitTreeDestroy(PrJsKitTree* PR_NULLABLE tree) {
    if (tree == nullptr)
        return;
    for (uint32_t i = 0; i < tree->nodes.count; ++i)
        prRawStringReset(&tree->nodes.data[i].text);
    tree->nodes.reset();
    tree->free.reset();
    tree->events.reset();
    prMemoryFree(tree);
}

void prJsKitTreeBind(PrJsKitTree* tree, JSContext* ctx) {
    PR_ASSERT(tree->ctx == nullptr);
    tree->ctx = ctx;
    JSClassDef definition = {.class_name = "KitTree"};
    jsRegisterClass(JS_GetRuntime(ctx), &kitTreeClass, definition);
    JSValue object = JS_NewObjectClass(ctx, int(kitTreeClass));
    JS_SetOpaque(object, static_cast<JsKitTree*>(tree));
    JS_SetPropertyFunctionList(ctx, object, kTreeFunctions, int(countOf(kTreeFunctions)));
    JSValue global = JS_GetGlobalObject(ctx);
    JS_DefinePropertyValueStr(ctx, global, "kitTree", object, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_FreeValue(ctx, global);
}

void prJsKitTreeBuild(PrJsKitTree* tree, PrKit* kit, PrLayoutNode parent) {
    // A round that was built and never dispatched leaves its events behind: the last round's stand.
    tree->events.clear();
    declareChildren(tree, kit, parent, parent, 0);
}

bool prJsKitTreeDispatch(PrJsKitTree* tree) {
    JSContext* ctx = tree->ctx;
    if (ctx == nullptr || tree->events.count == 0) {
        tree->events.clear();
        return false;
    }
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue object = JS_GetPropertyStr(ctx, global, "kitTree");
    JSValue listener = JS_GetPropertyStr(ctx, object, "onevent");
    bool called = false;
    if (JS_IsFunction(ctx, listener)) {
        // The listener may destroy nodes, and their slots stay unused until the queue is empty (nodeCreate), so each
        // event still names the node that it was queued for, or none.
        for (uint32_t i = 0; i < tree->events.count; ++i) {
            JsKitEvent event = tree->events.data[i];
            PrStringView name = keywordName(kEvents, event.type);
            JSValue arguments[2] = {JS_NewUint32(ctx, event.node), JS_NewStringLen(ctx, name.data, name.size)};
            JSValue result = JS_Call(ctx, listener, object, 2, arguments);
            if (JS_IsException(result))
                reportException(ctx);
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, arguments[1]);
            called = true;
        }
    }
    JS_FreeValue(ctx, listener);
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, global);
    tree->events.clear();
    return called;
}
