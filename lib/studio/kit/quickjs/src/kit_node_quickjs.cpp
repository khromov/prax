// The tree of nodes that a script edits, the values that their attributes are read from, and the walk that declares the
// tree in a round.

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

#include "pr/alloc.h"
#include "pr/assert.h"
#include "pr/count_of.h"
#include "pr/kit_button.h"
#include "pr/kit_text.h"
#include "pr/mem.h"
#include "pr/memory.h"
#include "pr/raw_array.h"

#include "studio/kit/quickjs/src/kit_block_quickjs.h"
#include "studio/kit/quickjs/src/kit_box_quickjs.h"
#include "studio/kit/quickjs/src/kit_button_quickjs.h"
#include "studio/kit/quickjs/src/kit_choice_quickjs.h"
#include "studio/kit/quickjs/src/kit_collection_quickjs.h"
#include "studio/kit/quickjs/src/kit_color_quickjs.h"
#include "studio/kit/quickjs/src/kit_data_quickjs.h"
#include "studio/kit/quickjs/src/kit_dock_quickjs.h"
#include "studio/kit/quickjs/src/kit_graph_quickjs.h"
#include "studio/kit/quickjs/src/kit_icon_quickjs.h"
#include "studio/kit/quickjs/src/kit_image_quickjs.h"
#include "studio/kit/quickjs/src/kit_input_quickjs.h"
#include "studio/kit/quickjs/src/kit_menu_quickjs.h"
#include "studio/kit/quickjs/src/kit_panel_quickjs.h"
#include "studio/kit/quickjs/src/kit_text_quickjs.h"
#include "studio/kit/quickjs/src/kit_timeline_quickjs.h"
#include "studio/kit/quickjs/src/kit_title_bar_quickjs.h"

namespace pr_impl {

// One node. A freed node has no kind, and a later create reuses it.
struct JsKitNode {
    const JsKitKind* PR_NULLABLE kind;
    void* PR_NULLABLE data;         // the kind's data, owned

    // The node's place in the tree, each kJsKitNodeNone if there is none.
    JsKitNodeIndex parent;
    JsKitNodeIndex firstChild;
    JsKitNodeIndex lastChild;
    JsKitNodeIndex previous;
    JsKitNodeIndex next;

    PrRawString text;
    PrKitPlace place;
    bool placed;                    // the script set an attribute of `place`
    uint32_t listens;               // a bit for each of the kind's events, by its index, that the script listens for
};

// A queued event. Its text is a range of the tree's event text.
struct JsKitStoredEvent {
    JsKitNodeIndex node;
    PrStringView type;
    JsKitDetail detail;
    uint32_t textStart;
};

// The nodes that a script made, and the events of the last round that it has yet to hear of.
struct JsKitNodeTree {
    RawArray<JsKitNode> nodes;          // by JsKitNodeIndex
    RawArray<JsKitNodeIndex> free;      // the freed nodes
    RawArray<JsKitStoredEvent> events;
    RawArray<char> eventText;           // the texts of the events' details, one after another
    bool changed;
    uint64_t version;                   // the number of edits that set `changed` so far
};

namespace {

// The root's kind, which declares nothing: the host declares the root's children under a node of its own.
PrLayoutNode rootDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kRootKind = {.name = "", .declare = rootDeclare};

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
    {"axis-x", PrKitTone_AxisX},
    {"axis-y", PrKitTone_AxisY},
    {"axis-z", PrKitTone_AxisZ},
    {"axis-w", PrKitTone_AxisW},
};

constexpr JsKitKeyword kAlignments[] = {
    {"auto", PrKitAlign_Auto},
    {"stretch", PrKitAlign_Stretch},
    {"start", PrKitAlign_Start},
    {"end", PrKitAlign_End},
    {"center", PrKitAlign_Center},
    {"baseline", PrKitAlign_Baseline},
};

constexpr JsKitKeyword kControlSizes[] = {
    {"md", PrKitControlSize_Md},
    {"sm", PrKitControlSize_Sm},
    {"xs", PrKitControlSize_Xs},
    {"lg", PrKitControlSize_Lg},
};

constexpr JsKitKeyword kFlexes[] = {
    {"own", PrKitFlex_Own},
    {"auto", PrKitFlex_Auto},
    {"fixed", PrKitFlex_Fixed},
    {"fill", PrKitFlex_Fill},
    {"grow", PrKitFlex_Grow},
};

constexpr JsKitKeyword kColors[] = {
    {"accent", JsKitColor_Accent},
    {"accent-weak", JsKitColor_AccentWeak},
    {"text-selection", JsKitColor_TextSelection},
    {"accent-text", JsKitColor_AccentText},
    {"accent-hover", JsKitColor_AccentHover},
    {"accent-press", JsKitColor_AccentPress},
    {"success", JsKitColor_Success},
    {"warning", JsKitColor_Warning},
    {"danger", JsKitColor_Danger},
    {"bg0", JsKitColor_Bg0},
    {"bg1", JsKitColor_Bg1},
    {"bg2", JsKitColor_Bg2},
    {"bg3", JsKitColor_Bg3},
    {"bg4", JsKitColor_Bg4},
    {"bg-viewport", JsKitColor_BgViewport},
    {"line1", JsKitColor_Line1},
    {"line2", JsKitColor_Line2},
    {"line3", JsKitColor_Line3},
    {"text1", JsKitColor_Text1},
    {"text2", JsKitColor_Text2},
    {"text3", JsKitColor_Text3},
    {"text4", JsKitColor_Text4},
    {"text-inverse", JsKitColor_TextInverse},
    {"hud-fill", JsKitColor_HudFill},
    {"hud-border", JsKitColor_HudBorder},
    {"axis-x", JsKitColor_AxisX},
    {"axis-y", JsKitColor_AxisY},
    {"axis-z", JsKitColor_AxisZ},
    {"axis-w", JsKitColor_AxisW},
};

// Reads one to four lengths separated by spaces, as CSS's shorthands give the edges of a box.
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
        if (count == 4 || !jsKitSetLength(PrStringView{text.data + at, end - at}, false, &values[count]))
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

// Sets an attribute of how a node sits in its parent. Returns false if `name` is not one, or `value` does not suit it.
bool setPlacement(PrKitPlace& place, PrStringView name, PrStringView value, bool reset) {
    if (name == "width")
        return jsKitSetSize(value, reset, &place.width);
    if (name == "height")
        return jsKitSetSize(value, reset, &place.height);
    if (name == "min-width")
        return jsKitSetSize(value, reset, &place.minWidth);
    if (name == "min-height")
        return jsKitSetSize(value, reset, &place.minHeight);
    if (name == "max-width")
        return jsKitSetSize(value, reset, &place.maxWidth);
    if (name == "max-height")
        return jsKitSetSize(value, reset, &place.maxHeight);
    if (name == "margin")
        return jsKitSetEdges(value, reset, &place.margin);
    if (name == "flex")
        return jsKitSetKeyword(kFlexes, value, reset, &place.flex);
    if (name == "align-self")
        return jsKitSetKeyword(kAlignments, value, reset, &place.align);
    if (name == "weight")
        return jsKitSetLength(value, reset, &place.weight);
    return false;
}

// The families, whose kinds jsKitKindFrom searches in this order.
typedef Span<const JsKitKind> (*FamilyFn)();
constexpr FamilyFn kFamilies[] = {
    jsKitBoxKinds, jsKitTextKinds, jsKitButtonKinds, jsKitPanelKinds, jsKitIconKinds, jsKitImageKinds, jsKitChoiceKinds,
    jsKitMenuKinds, jsKitInputKinds, jsKitCollectionKinds, jsKitDataKinds, jsKitColorKinds, jsKitGraphKinds,
    jsKitTimelineKinds, jsKitDockKinds, jsKitTitleBarKinds, jsKitBlockKinds,
};

// Detaches a node from its parent, if it has one.
void unlink(JsKitNodeTree* tree, JsKitNodeIndex index) {
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

// Frees a detached node and the nodes under it.
void freeNode(JsKitNodeTree* tree, JsKitNodeIndex index) {
    JsKitNodeIndex child = tree->nodes.data[index].firstChild;
    while (child != kJsKitNodeNone) {
        JsKitNodeIndex next = tree->nodes.data[child].next;
        freeNode(tree, child);
        child = next;
    }
    JsKitNode& node = tree->nodes.data[index];
    if (node.data != nullptr && node.kind->destroy != nullptr)
        node.kind->destroy(node.data);
    prMemoryFree(node.data);
    prRawStringReset(&node.text);
    node = JsKitNode{};
    tree->free.push(index);
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// Values

bool jsKitKeywordFrom(Span<const JsKitKeyword> keywords, PrStringView text, uint8_t* out) {
    for (const JsKitKeyword& keyword : keywords) {
        if (keyword.name == text) {
            *out = keyword.value;
            return true;
        }
    }
    return false;
}

PrStringView jsKitKeywordName(Span<const JsKitKeyword> keywords, uint8_t value) {
    for (const JsKitKeyword& keyword : keywords) {
        if (keyword.value == value)
            return keyword.name;
    }
    return "";
}

bool jsKitNumberFrom(PrStringView text, double* out) {
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
            break;
        mantissa = mantissa * 10 + (c - '0');
        if (fraction)
            divisor *= 10;
        digits = true;
    }
    if (!digits)
        return false;
    double value = mantissa / divisor;

    // An exponent, which is how a script's number reads once it is very small or very large.
    if (at < text.size && (text.data[at] == 'e' || text.data[at] == 'E')) {
        ++at;
        bool shrinks = false;
        if (at < text.size && (text.data[at] == '-' || text.data[at] == '+'))
            shrinks = text.data[at++] == '-';
        uint32_t exponent = 0;
        bool exponentDigits = false;
        for (; at < text.size && text.data[at] >= '0' && text.data[at] <= '9' && exponent < 400; ++at) {
            exponent = exponent * 10 + uint32_t(text.data[at] - '0');
            exponentDigits = true;
        }
        if (!exponentDigits)
            return false;
        for (uint32_t i = 0; i < exponent; ++i)
            value = shrinks ? value / 10 : value * 10;
    }
    if (at != text.size)
        return false;
    *out = negative ? -value : value;
    return true;
}

bool jsKitSetKeyword(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out) {
    if (reset) {
        *out = 0;
        return true;
    }
    return jsKitKeywordFrom(keywords, value, out);
}

bool jsKitSetNumber(PrStringView value, bool reset, double* out) {
    if (reset) {
        *out = 0;
        return true;
    }
    return jsKitNumberFrom(value, out);
}

bool jsKitSetFloat(PrStringView value, bool reset, float* out) {
    double number = 0;
    if (!jsKitSetNumber(value, reset, &number))
        return false;
    *out = float(number);
    return true;
}

bool jsKitSetLength(PrStringView value, bool reset, float* out) {
    double number = 0;
    if (!jsKitSetNumber(value, reset, &number) || number < 0)
        return false;
    *out = float(number);
    return true;
}

bool jsKitSetUint(PrStringView value, bool reset, uint32_t* out) {
    double number = 0;
    if (!jsKitSetNumber(value, reset, &number) || number < 0 || number > 4294967295.0 ||
            number != double(uint32_t(number)))
        return false;
    *out = uint32_t(number);
    return true;
}

bool jsKitSetFlag(PrStringView value, bool reset, bool* out) {
    if (reset || value == "false")
        *out = false;
    else if (value == "" || value == "true")
        *out = true;
    else
        return false;
    return true;
}

bool jsKitSetEdges(PrStringView value, bool reset, PrKitEdges* out) {
    if (reset) {
        *out = PrKitEdges{};
        return true;
    }
    return edgesFrom(value, out);
}

bool jsKitSetSize(PrStringView value, bool reset, PrKitSize* out) {
    float number = 0;
    if (reset || value == "auto") {
        *out = PrKitSize{};
    } else if (value == "fill") {
        *out = prKitFill();
    } else if (value.size > 1 && value.data[value.size - 1] == '%') {
        if (!jsKitSetLength(PrStringView{value.data, value.size - 1}, false, &number))
            return false;
        *out = prKitPercent(number);
    } else {
        if (!jsKitSetLength(value, false, &number))
            return false;
        *out = prKitPx(number);
    }
    return true;
}

bool jsKitSetString(PrStringView value, bool reset, PrRawString* out) {
    if (reset)
        prRawStringReset(out);
    else
        prRawStringReplace(out, value);
    return true;
}

bool jsKitSetIcon(PrStringView value, bool reset, PrKitIconKind* out) {
    if (reset) {
        *out = PrKitIconKind_None;
        return true;
    }
    for (uint32_t icon = PrKitIconKind_None + 1; icon < PrKitIconKind_Count; ++icon) {
        if (prKitIconName(PrKitIconKind(icon)) == value) {
            *out = PrKitIconKind(icon);
            return true;
        }
    }
    return false;
}

Span<const JsKitKeyword> jsKitRoles() {
    return kRoles;
}

Span<const JsKitKeyword> jsKitTones() {
    return kTones;
}

Span<const JsKitKeyword> jsKitAlignments() {
    return kAlignments;
}

Span<const JsKitKeyword> jsKitControlSizes() {
    return kControlSizes;
}

Span<const JsKitKeyword> jsKitColors() {
    return kColors;
}

PrColor jsKitColorFrom(const PrKit& kit, JsKitColor color) {
    const PrKitPalette& palette = kit.theme.palette;
    switch (color) {
        case JsKitColor_Accent:
            return palette.accent;
        case JsKitColor_AccentWeak:
            return palette.accentWeak;
        case JsKitColor_TextSelection:
            return palette.textSelection;
        case JsKitColor_AccentText:
            return palette.accentText;
        case JsKitColor_AccentHover:
            return palette.accentHover;
        case JsKitColor_AccentPress:
            return palette.accentPress;
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
        case JsKitColor_BgViewport:
            return palette.bgViewport;
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
        case JsKitColor_HudFill:
            return palette.hudFill;
        case JsKitColor_HudBorder:
            return palette.hudBorder;
        case JsKitColor_AxisX:
            return palette.axisX;
        case JsKitColor_AxisY:
            return palette.axisY;
        case JsKitColor_AxisZ:
            return palette.axisZ;
        case JsKitColor_AxisW:
            return palette.axisW;
        default:
            return PrColor{};
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Nodes

const JsKitKind* jsKitNodeKind(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].kind;
}

void* PR_NULLABLE jsKitNodeData(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].data;
}

PrStringView jsKitNodeText(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return prRawStringView(&tree->nodes.data[node].text);
}

JsKitNodeIndex jsKitNodeParent(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].parent;
}

JsKitNodeIndex jsKitNodeFirstChild(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].firstChild;
}

JsKitNodeIndex jsKitNodeNextSibling(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].next;
}

PrKitPlace jsKitNodePlace(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].place;
}

bool jsKitNodeIsPlaced(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return tree->nodes.data[node].placed;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rounds

PrUiKey jsKitNodeKey(const JsKitRound& round, JsKitNodeIndex node, uint32_t part) {
    if (part == 0)
        return prUiKeyIndex(round.kit->ui, round.base, node);
    return prUiKeyIndex2(round.kit->ui, round.base, node, part);
}

bool jsKitNodeListens(const JsKitNodeTree* tree, JsKitNodeIndex node, uint32_t event) {
    return (tree->nodes.data[node].listens & (1u << event)) != 0;
}

void jsKitEventPush(const JsKitRound& round, JsKitNodeIndex node, uint32_t event, const JsKitDetail* PR_NULLABLE detail) {
    JsKitNodeTree* tree = round.tree;
    PR_ASSERT(event < tree->nodes.data[node].kind->eventCount);
    if (!jsKitNodeListens(tree, node, event))
        return;
    JsKitStoredEvent stored{};
    stored.node = node;
    stored.type = tree->nodes.data[node].kind->events[event];
    if (detail != nullptr) {
        stored.detail = *detail;
        stored.textStart = tree->eventText.count;
        if (detail->hasText)
            tree->eventText.append(detail->text.data, uint32_t(detail->text.size));
    }
    tree->events.push(stored);
}

void jsKitDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitKind* kind = round.tree->nodes.data[node].kind;
    PrLayoutNode made = kind->declare(round, node, parent);
    if (made == kPrLayoutNodeNone)
        return;
    const JsKitNode& declared = round.tree->nodes.data[node];
    if (declared.placed && (kind->flags & JsKitKindFlag_PlacesItself) == 0)
        prKitPlace(round.kit, made, declared.place);
    jsKitDeclareChildren(round, node, made);
}

void jsKitDeclareChildren(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    for (JsKitNodeIndex child = round.tree->nodes.data[node].firstChild; child != kJsKitNodeNone;
            child = round.tree->nodes.data[child].next)
        jsKitDeclare(round, child, parent);
}

// ---------------------------------------------------------------------------------------------------------------------
// Kinds

const JsKitKind* PR_NULLABLE jsKitKindFrom(PrStringView name) {
    for (FamilyFn family : kFamilies) {
        for (const JsKitKind& kind : family()) {
            if (kind.name == name)
                return &kind;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Trees

JsKitNodeTree* jsKitTreeCreate() {
    JsKitNodeTree* tree = allocZeroed<JsKitNodeTree>();
    jsKitNodeCreate(tree, &kRootKind);
    tree->changed = false;
    return tree;
}

void jsKitTreeDestroy(JsKitNodeTree* tree) {
    for (uint32_t i = 0; i < tree->nodes.count; ++i) {
        JsKitNode& node = tree->nodes.data[i];
        if (node.data != nullptr && node.kind->destroy != nullptr)
            node.kind->destroy(node.data);
        prMemoryFree(node.data);
        prRawStringReset(&node.text);
    }
    tree->nodes.reset();
    tree->free.reset();
    tree->events.reset();
    tree->eventText.reset();
    prMemoryFree(tree);
}

bool jsKitTreeHas(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return node < tree->nodes.count && tree->nodes.data[node].kind != nullptr;
}

bool jsKitTreeIsChanged(const JsKitNodeTree* tree) {
    return tree->changed;
}

uint64_t jsKitTreeVersion(const JsKitNodeTree* tree) {
    return tree->version;
}

JsKitNodeIndex jsKitNodeCreate(JsKitNodeTree* tree, const JsKitKind* kind) {
    JsKitNodeIndex index = tree->nodes.count;
    if (tree->free.count > 0 && tree->events.count == 0) {
        index = tree->free.last();
        tree->free.pop();
    } else {
        tree->nodes.emplace();
    }
    JsKitNode node{};
    node.kind = kind;
    node.parent = kJsKitNodeNone;
    node.firstChild = kJsKitNodeNone;
    node.lastChild = kJsKitNodeNone;
    node.previous = kJsKitNodeNone;
    node.next = kJsKitNodeNone;
    if (kind->dataSize > 0) {
        node.data = prMemoryAllocZeroed(kind->dataSize);
        if (kind->create != nullptr)
            kind->create(node.data);
    }
    tree->nodes.data[index] = node;
    tree->changed = true;
    ++tree->version;
    return index;
}

void jsKitNodeDestroy(JsKitNodeTree* tree, JsKitNodeIndex node) {
    unlink(tree, node);
    freeNode(tree, node);
    tree->changed = true;
    ++tree->version;
}

bool jsKitNodeIsWithin(const JsKitNodeTree* tree, JsKitNodeIndex node, JsKitNodeIndex ancestor) {
    for (; node != kJsKitNodeNone; node = tree->nodes.data[node].parent) {
        if (node == ancestor)
            return true;
    }
    return false;
}

void jsKitNodeInsert(JsKitNodeTree* tree, JsKitNodeIndex parent, JsKitNodeIndex node, JsKitNodeIndex before) {
    unlink(tree, node);
    JsKitNode& above = tree->nodes.data[parent];
    JsKitNode& moved = tree->nodes.data[node];
    moved.parent = parent;
    moved.next = before;
    if (before != kJsKitNodeNone) {
        moved.previous = tree->nodes.data[before].previous;
        tree->nodes.data[before].previous = node;
    } else {
        moved.previous = above.lastChild;
        above.lastChild = node;
    }
    if (moved.previous != kJsKitNodeNone)
        tree->nodes.data[moved.previous].next = node;
    else
        above.firstChild = node;
    tree->changed = true;
    ++tree->version;
}

void jsKitNodeRemove(JsKitNodeTree* tree, JsKitNodeIndex node) {
    unlink(tree, node);
    tree->changed = true;
    ++tree->version;
}

bool jsKitNodeSet(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView name, PrStringView value, bool reset) {
    JsKitNode& changed = tree->nodes.data[node];
    if (node == 0)
        return false;
    PrKitPlace place = changed.place;
    if (setPlacement(place, name, value, reset)) {
        changed.place = place;
        changed.placed = true;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (changed.kind->set == nullptr || !changed.kind->set(changed.data, name, value, reset))
        return false;
    tree->changed = true;
    ++tree->version;
    return true;
}

void jsKitNodeSetText(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView text) {
    prRawStringReplace(&tree->nodes.data[node].text, text);
    tree->changed = true;
    ++tree->version;
}

bool jsKitNodeListen(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView type, bool listening) {
    JsKitNode& heard = tree->nodes.data[node];
    for (uint32_t event = 0; event < heard.kind->eventCount; ++event) {
        if (heard.kind->events[event] != type)
            continue;
        if (listening)
            heard.listens |= 1u << event;
        else
            heard.listens &= ~(1u << event);
        return true;
    }
    return false;
}

void jsKitTreeDeclare(JsKitNodeTree* tree, PrKit* kit, PrLayoutNode parent) {
    jsKitTreeClearEvents(tree);
    tree->changed = false;
    JsKitRound round{.tree = tree, .kit = kit, .base = parent};
    jsKitDeclareChildren(round, 0, parent);
}

uint32_t jsKitTreeEventCount(const JsKitNodeTree* tree) {
    return tree->events.count;
}

JsKitQueuedEvent jsKitTreeEvent(const JsKitNodeTree* tree, uint32_t index) {
    const JsKitStoredEvent& stored = tree->events.data[index];
    JsKitQueuedEvent event{.node = stored.node, .type = stored.type, .detail = stored.detail};
    if (stored.detail.hasText)
        event.detail.text = PrStringView{tree->eventText.data + stored.textStart, stored.detail.text.size};
    return event;
}

void jsKitTreeClearEvents(JsKitNodeTree* tree) {
    tree->events.clear();
    tree->eventText.clear();
}

} // namespace pr_impl
