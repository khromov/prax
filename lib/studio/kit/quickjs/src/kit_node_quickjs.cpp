// The tree of nodes that a script edits, the values that their attributes are read from, and the walk that declares the
// tree in a round.

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

#include "pr/alloc.h"
#include "pr/ascii.h"
#include "pr/assert.h"
#include "pr/count_of.h"
#include "pr/float.h"
#include "pr/float_convert.h"
#include "pr/integer.h"
#include "pr/kit_button.h"
#include "pr/kit_color.h"
#include "pr/kit_text.h"
#include "pr/layout_query.h"
#include "pr/layout_unit.h"
#include "pr/mem.h"
#include "pr/memory.h"
#include "pr/raw_array.h"

#include "studio/kit/quickjs/src/kit_block_quickjs.h"
#include "studio/kit/quickjs/src/kit_box_quickjs.h"
#include "studio/kit/quickjs/src/kit_choice_quickjs.h"
#include "studio/kit/quickjs/src/kit_collection_quickjs.h"
#include "studio/kit/quickjs/src/kit_color_quickjs.h"
#include "studio/kit/quickjs/src/kit_dock_quickjs.h"
#include "studio/kit/quickjs/src/kit_graph_quickjs.h"
#include "studio/kit/quickjs/src/kit_icon_quickjs.h"
#include "studio/kit/quickjs/src/kit_image_quickjs.h"
#include "studio/kit/quickjs/src/kit_input_quickjs.h"
#include "studio/kit/quickjs/src/kit_menu_quickjs.h"
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
    PrRawString key;                // the name that the script gave the node's key, or null for a key of its own
    PrKitPlace place;
    bool placed;                    // the script set an attribute of `place`
    bool revealPending;             // the script set `reveal`, and no round has brought the widget into view since
    bool focusPending;              // the script set `focus`, and no round has asked for the widget's focus since
    bool motion;                    // the script set `motion`
    bool listensBounds;             // the script listens for `bounds`
    bool listensRound;              // the script listens for `round`, so the node is among the tree's `hearing`
    uint32_t listens;               // a bit for each of the kind's events, by its index, that the script listens for
    PrRect bounds;                  // what the last `bounds` event told, in px of the viewport

    // The payload that a press on the node's widget picks up, which is none while `dragKind` is zero. The press picks
    // it up once it has moved `dragThreshold` px.
    uint64_t dragKind;
    uint64_t dragValue;
    float dragThreshold;
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
    RawArray<JsKitNodeIndex> hearing;   // the nodes that the script listens for `round` on
    PrJsKitTheme theme;                 // what the script set the root's `theme` to
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

constexpr JsKitKeyword kThemes[] = {
    {"system", PrJsKitTheme_System},
    {"dark", PrJsKitTheme_Dark},
    {"light", PrJsKitTheme_Light},
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

// Returns the next word of `text` at or after `*at`, and moves `*at` past it. Spaces separate words. Returns an empty
// view once no word is left.
PrStringView nextWord(PrStringView text, size_t* at) {
    while (*at < text.size && text.data[*at] == ' ')
        ++*at;
    size_t start = *at;
    while (*at < text.size && text.data[*at] != ' ')
        ++*at;
    return PrStringView{text.data + start, *at - start};
}

// Reads the digits of a color in hexadecimal notation, which follow its `#` (css-color-4/#hex-notation).
bool hexFrom(PrStringView digits, PrColor* out) {
    if (digits.size != 3 && digits.size != 4 && digits.size != 6 && digits.size != 8)
        return false;
    for (size_t i = 0; i < digits.size; ++i) {
        if (!prAsciiIsHexDigit(digits.data[i]))
            return false;
    }
    // The short notations give one digit for each channel, which stands for the same digit twice.
    size_t width = digits.size < 6 ? 1 : 2;
    uint8_t channels[4] = {0, 0, 0, 255};
    for (size_t channel = 0; channel < digits.size / width; ++channel) {
        uint32_t high = prAsciiHexValue(digits.data[channel * width]);
        uint32_t low = width == 2 ? prAsciiHexValue(digits.data[channel * width + 1]) : high;
        channels[channel] = uint8_t(high * 16 + low);
    }
    *out = prColorFromColor8(PrColor8{channels[0], channels[1], channels[2], channels[3]});
    return true;
}

// Reads three or four numbers separated by spaces as a color's red, green, blue and alpha, with an alpha of 1 if the
// text has none.
bool channelsFrom(PrStringView text, PrColor* out) {
    float channels[4] = {0, 0, 0, 1};
    uint32_t count = 0;
    size_t at = 0;
    for (PrStringView word = nextWord(text, &at); word.size > 0; word = nextWord(text, &at)) {
        if (count == 4 || !jsKitSetFloat(word, false, &channels[count]))
            return false;
        ++count;
    }
    if (count < 3)
        return false;
    *out = PrColor{channels[0], channels[1], channels[2], channels[3]};
    return true;
}

// Reads the name of a color of the palette, a space, and the color's shade, a number from -1 to 1. Returns false, and
// writes nothing, for any other text.
bool shadedFrom(PrStringView text, JsKitColorValue* out) {
    size_t space = 0;
    while (space < text.size && text.data[space] != ' ')
        ++space;
    JsKitColor named = JsKitColor_None;
    float shade = 0;
    if (space == text.size || !jsKitKeywordFrom(jsKitColors(), PrStringView{text.data, space}, &named) ||
            !jsKitSetFloat(PrStringView{text.data + space + 1, text.size - space - 1}, false, &shade) ||
            !(shade >= -1 && shade <= 1))
        return false;
    out->named = named;
    out->shade = shade;
    return true;
}

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

// The distance in px that a press moves before it picks up a payload whose node gives no `drag-threshold`. The kit's
// own sources, such as the rows of a tree view, pick theirs up at the same distance.
constexpr float kDragThreshold = 4;

// Sets an attribute of the payload that a node's widget is the source of. Returns false if `name` is not one, or
// `value` does not suit it. The kind of the system's content is refused, since no widget is the source of that.
bool setDragSource(JsKitNode& node, PrStringView name, PrStringView value, bool reset) {
    if (name == "drag-source" || name == "drag-value") {
        uint64_t number = 0;
        if (!reset && !prU64Parse(value.data, value.size, &number, 10))
            return false;
        if (name == "drag-value") {
            node.dragValue = number;
            return true;
        }
        if (number == kPrUiPayloadKindSystem)
            return false;
        node.dragKind = number;
        return true;
    }
    if (name == "drag-threshold") {
        float threshold = kDragThreshold;
        if (!reset && !jsKitSetLength(value, false, &threshold))
            return false;
        node.dragThreshold = threshold;
        return true;
    }
    return false;
}

// The families, whose kinds jsKitKindFrom searches in this order.
typedef Span<const JsKitKind> (*FamilyFn)();
constexpr FamilyFn kFamilies[] = {
    jsKitDescribedKinds, jsKitBoxKinds, jsKitTextKinds, jsKitIconKinds, jsKitImageKinds, jsKitChoiceKinds,
    jsKitMenuKinds, jsKitInputKinds, jsKitCollectionKinds, jsKitColorKinds, jsKitGraphKinds, jsKitTimelineKinds,
    jsKitDockKinds, jsKitTitleBarKinds, jsKitBlockKinds,
};

// Returns the field of type `Field` at `offset` in a node's data.
template <typename Field>
Field* fieldAt(void* data, uint32_t offset) {
    return static_cast<Field*>(static_cast<void*>(static_cast<char*>(data) + offset));
}

// Sets or resets the attribute `name` of a node's data, if the kind holds it as a field. Returns false, leaving the
// data as it was, if it holds no such field or `value` does not suit the field.
bool setField(const JsKitKind& kind, void* data, PrStringView name, PrStringView value, bool reset) {
    for (uint32_t i = 0; i < kind.fieldCount; ++i) {
        const JsKitField& field = kind.fields[i];
        if (field.name != name)
            continue;
        switch (field.type) {
            case JsKitFieldType_Keyword: {
                Span<const JsKitKeyword> keywords(field.keywords, field.keywordCount);
                return jsKitSetKeyword(keywords, value, reset, fieldAt<uint8_t>(data, field.offset));
            }
            case JsKitFieldType_Keywords: {
                Span<const JsKitKeyword> keywords(field.keywords, field.keywordCount);
                return jsKitSetKeywords(keywords, value, reset, fieldAt<uint8_t>(data, field.offset));
            }
            case JsKitFieldType_Flag:
                return jsKitSetFlag(value, reset, fieldAt<bool>(data, field.offset));
            case JsKitFieldType_Float:
                return jsKitSetFloat(value, reset, fieldAt<float>(data, field.offset));
            case JsKitFieldType_Length:
                return jsKitSetLength(value, reset, fieldAt<float>(data, field.offset));
            case JsKitFieldType_Uint:
                return jsKitSetUint(value, reset, fieldAt<uint32_t>(data, field.offset));
            case JsKitFieldType_Edges:
                return jsKitSetEdges(value, reset, fieldAt<PrKitEdges>(data, field.offset));
            case JsKitFieldType_Size:
                return jsKitSetSize(value, reset, fieldAt<PrKitSize>(data, field.offset));
            case JsKitFieldType_Icon:
                return jsKitSetIcon(value, reset, fieldAt<PrKitIconKind>(data, field.offset));
            case JsKitFieldType_Color:
                return jsKitSetColorValue(value, reset, fieldAt<JsKitColorValue>(data, field.owner));
            case JsKitFieldType_String: {
                PrRawString* owner = fieldAt<PrRawString>(data, field.owner);
                jsKitSetString(value, reset, owner);
                *fieldAt<PrStringView>(data, field.offset) = prRawStringView(owner);
                return true;
            }
            default:
                return false;
        }
    }
    return false;
}

// Frees a node's data, after the kind has released what the data owns.
void freeData(JsKitNode& node) {
    if (node.data == nullptr)
        return;
    for (uint32_t i = 0; i < node.kind->fieldCount; ++i) {
        if (node.kind->fields[i].type == JsKitFieldType_String)
            prRawStringReset(fieldAt<PrRawString>(node.data, node.kind->fields[i].owner));
    }
    if (node.kind->destroy != nullptr)
        node.kind->destroy(node.data);
    prMemoryFree(node.data);
    node.data = nullptr;
}

// The event that a node of any kind queues when its widget's box moves or changes size.
constexpr PrStringView kBoundsEvent = "bounds";

// The event that tells a node of any kind the round that is about to be declared.
constexpr PrStringView kRoundEvent = "round";

// Queues an event of `type`, which outlives the tree, for the script, with what `detail` tells, if it is not null.
void queueEvent(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView type, const JsKitDetail* PR_NULLABLE detail) {
    JsKitStoredEvent stored{};
    stored.node = node;
    stored.type = type;
    if (detail != nullptr) {
        stored.detail = *detail;
        stored.textStart = tree->eventText.count;
        if (detail->hasText)
            tree->eventText.append(detail->text.data, uint32_t(detail->text.size));
    }
    tree->events.push(stored);
}

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
    freeData(node);
    prRawStringReset(&node.text);
    prRawStringReset(&node.key);
    if (node.listensRound)
        tree->hearing.removeFirst(index);
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

bool jsKitSetKeywords(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out) {
    uint8_t all = 0;
    size_t at = 0;
    while (!reset && at < value.size) {
        if (value.data[at] == ' ') {
            ++at;
            continue;
        }
        size_t end = at;
        while (end < value.size && value.data[end] != ' ')
            ++end;
        uint8_t one = 0;
        if (!jsKitKeywordFrom(keywords, PrStringView{value.data + at, end - at}, &one))
            return false;
        all |= one;
        at = end;
    }
    if (!reset && all == 0)
        return false;
    *out = all;
    return true;
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

bool jsKitSetColor(PrStringView value, bool reset, PrColor* out) {
    if (reset) {
        *out = PrColor{};
        return true;
    }
    if (prKitColorParseHex(value, out))
        return true;
    float channels[4] = {0, 0, 0, 1};
    uint32_t count = 0;
    size_t at = 0;
    while (at < value.size) {
        if (value.data[at] == ' ') {
            ++at;
            continue;
        }
        size_t end = at;
        while (end < value.size && value.data[end] != ' ')
            ++end;
        double channel = 0;
        if (count == 4 || prF64ParsePrefix(PrStringView{value.data + at, end - at}, &channel) != end - at ||
                !prF64IsFinite(channel))
            return false;
        channels[count++] = float(channel);
        at = end;
    }
    if (count < 3)
        return false;
    *out = PrColor{channels[0], channels[1], channels[2], channels[3]};
    return true;
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

bool jsKitSetColorValue(PrStringView value, bool reset, JsKitColorValue* out) {
    JsKitColorValue color{};
    if (!reset) {
        if (value.size > 0 && value.data[0] == '#') {
            if (!hexFrom(PrStringView{value.data + 1, value.size - 1}, &color.own))
                return false;
        } else if (!jsKitKeywordFrom(jsKitColors(), value, &color.named) && !shadedFrom(value, &color) &&
                !channelsFrom(value, &color.own)) {
            return false;
        }
    }
    *out = color;
    return true;
}

PrColor jsKitColorFromValue(const PrKit& kit, const JsKitColorValue& value) {
    if (value.named == JsKitColor_None)
        return value.own;
    PrColor color = jsKitColorFrom(kit, value.named);
    if (value.shade == 0)
        return color;
    float target = value.shade > 0 ? 1.0f : 0.0f;
    float share = value.shade > 0 ? value.shade : -value.shade;
    return PrColor{
        color.r + (target - color.r) * share, color.g + (target - color.g) * share,
        color.b + (target - color.b) * share, color.a,
    };
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

bool jsKitNodeIsUnder(const JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView kind) {
    JsKitNodeIndex parent = tree->nodes.data[node].parent;
    return parent != kJsKitNodeNone && tree->nodes.data[parent].kind->name == kind;
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
    const PrRawString& named = round.tree->nodes.data[node].key;
    if (part == 0 && !prRawStringIsNull(&named))
        return prUiKey(round.kit->ui, round.base, prRawStringView(&named));
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
    if (jsKitNodeListens(tree, node, event))
        queueEvent(tree, node, tree->nodes.data[node].kind->events[event], detail);
}

void jsKitDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitKind* kind = round.tree->nodes.data[node].kind;
    PrLayoutNode made = kind->declare(round, node, parent);
    if (made == kPrLayoutNodeNone)
        return;
    JsKitNode& declared = round.tree->nodes.data[node];
    if (declared.placed && (kind->flags & JsKitKindFlag_PlacesItself) == 0)
        prKitPlace(round.kit, made, declared.place);
    if (declared.dragKind != 0)
        prUiDragStart(round.kit->ui, made, declared.dragThreshold, declared.dragKind, declared.dragValue, 0);
    if (declared.revealPending) {
        prUiReveal(round.kit->ui, made);
        declared.revealPending = false;
    }
    if (declared.focusPending) {
        // Focus moves as the round ends, so the widget shows it in a later round, which this asks a frame for.
        prUiSetFocus(round.kit->ui, made);
        prUiRequestFrame(round.kit->ui);
        declared.focusPending = false;
    }
    if (declared.motion)
        prUiRequestMotion(round.kit->ui, made);
    if (declared.listensBounds) {
        // The geometry of the last layout, which is what a program that declares its own widgets reads in a round.
        PrRect bounds = prLayoutRectToPx(prLayoutNodeBounds(round.kit->tree, made, PrLayoutCoordinates_Viewport));
        if (!prMemEquals(&bounds, &declared.bounds, sizeof(bounds))) {
            declared.bounds = bounds;
            JsKitDetail detail{
                .values = {double(bounds.left), double(bounds.top), double(bounds.right), double(bounds.bottom)},
                .valueCount = 4,
            };
            queueEvent(round.tree, node, kBoundsEvent, &detail);
        }
    }
    if ((kind->flags & JsKitKindFlag_DeclaresChildren) == 0)
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

PrLayoutNode jsKitWidgetDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    const JsKitKind& kind = *jsKitNodeKind(tree, node);
    const JsKitWidget& widget = *kind.widget;
    if (widget.under.size > 0 && !jsKitNodeIsUnder(tree, node, widget.under))
        return kPrLayoutNodeNone;
    void* data = jsKitNodeData(tree, node);
    if (widget.key != kJsKitOffsetNone)
        *fieldAt<PrUiKey>(data, widget.key) = jsKitNodeKey(round, node);
    if (widget.text != kJsKitOffsetNone)
        *fieldAt<PrStringView>(data, widget.text) = jsKitNodeText(tree, node);
    if (widget.place != kJsKitOffsetNone)
        *fieldAt<PrKitPlace>(data, widget.place) = jsKitNodePlace(tree, node);
    for (uint32_t i = 0; i < kind.fieldCount; ++i) {
        // A color follows the theme, which the host may have changed since the last round.
        const JsKitField& field = kind.fields[i];
        if (field.type == JsKitFieldType_Color) {
            const JsKitColorValue& color = *fieldAt<JsKitColorValue>(data, field.owner);
            *fieldAt<PrColor>(data, field.offset) = jsKitColorFromValue(*round.kit, color);
        }
    }
    PrLayoutNode made = widget.create(round.kit, parent, data);
    if (made == kPrLayoutNodeNone)
        return made;
    for (uint32_t event = 0; event < kind.eventCount; ++event) {
        if (widget.queries[event](round.kit, made))
            jsKitEventPush(round, node, event, nullptr);
    }
    return made;
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
        freeData(node);
        prRawStringReset(&node.text);
        prRawStringReset(&node.key);
    }
    tree->nodes.reset();
    tree->free.reset();
    tree->events.reset();
    tree->eventText.reset();
    tree->hearing.reset();
    prMemoryFree(tree);
}

bool jsKitTreeHas(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return node < tree->nodes.count && tree->nodes.data[node].kind != nullptr;
}

bool jsKitTreeIsChanged(const JsKitNodeTree* tree) {
    return tree->changed;
}

PrJsKitTheme jsKitTreeTheme(const JsKitNodeTree* tree) {
    return tree->theme;
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
    node.dragThreshold = kDragThreshold;
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
    if (node == 0) {
        // The root declares no widget, and takes the one attribute that is the whole tree's.
        if (name != "theme" || !jsKitSetKeyword(kThemes, value, reset, &tree->theme))
            return false;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (name == "key") {
        jsKitSetString(value, reset, &changed.key);
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (name == "reveal") {
        changed.revealPending = !reset;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (name == "focus") {
        changed.focusPending = !reset;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (name == "motion") {
        if (!jsKitSetFlag(value, reset, &changed.motion))
            return false;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (setDragSource(changed, name, value, reset)) {
        tree->changed = true;
        ++tree->version;
        return true;
    }
    PrKitPlace place = changed.place;
    if (setPlacement(place, name, value, reset)) {
        changed.place = place;
        changed.placed = true;
        tree->changed = true;
        ++tree->version;
        return true;
    }
    if (!setField(*changed.kind, changed.data, name, value, reset) &&
            (changed.kind->set == nullptr || !changed.kind->set(changed.data, name, value, reset)))
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
    if (type == kBoundsEvent) {
        // The first round that the script listens in tells it where the widget is.
        heard.listensBounds = listening;
        heard.bounds = PrRect{};
        return true;
    }
    if (type == kRoundEvent) {
        if (listening && !heard.listensRound)
            tree->hearing.push(node);
        else if (!listening && heard.listensRound)
            tree->hearing.removeFirst(node);
        heard.listensRound = listening;
        return true;
    }
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

bool jsKitTreeQueueRound(JsKitNodeTree* tree, const PrKit* kit) {
    if (tree->hearing.count == 0)
        return false;
    jsKitTreeClearEvents(tree);
    const PrUiRound* round = prUiCurrentRound(kit->ui);
    JsKitDetail detail{.values = {round->time, round->delta, double(round->index)}, .valueCount = 3};
    for (uint32_t i = 0; i < tree->hearing.count; ++i) {
        if (jsKitNodeIsWithin(tree, tree->hearing.data[i], 0))
            queueEvent(tree, tree->hearing.data[i], kRoundEvent, &detail);
    }
    return tree->events.count > 0;
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
