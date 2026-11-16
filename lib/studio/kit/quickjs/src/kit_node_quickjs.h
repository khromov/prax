// What the sources of Studio Kit's QuickJS binding share: the tree of nodes that a script edits, the kinds of node,
// each of which declares one of the kit's widgets, and the values that their attributes are read from.
//
// A kind is a description (JsKitKind) that a family's source defines, one source for each of the kit's headers. A node
// of a kind owns a block of the kind's data, which the kind's `set` writes from the attributes that the script sets and
// its `declare` reads in each round. The tree owns the node's place among the others, its text, how it sits in its
// parent, and which of its kind's events the script listens for.
//
// Most kinds need no source: the kit's function that creates the widget takes structs whose fields are the kind's
// attributes, so the kind is described by them (JsKitField, JsKitWidget). tools/gen/gen_kit.py writes those
// descriptions from the kit's headers.

#pragma once

#include "pr/base.h"
#include "pr/color.h"
#include "pr/kit.h"
#include "pr/kit_icon.h"
#include "pr/kit_icon_kind.h"
#include "pr/kit_quickjs.h"
#include "pr/kit_theme.h"
#include "pr/raw_string.h"
#include "pr/span.h"
#include "pr/string_view.h"
#include "pr/ui.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

// ---------------------------------------------------------------------------------------------------------------------
// Values
//
// An attribute's value is a string. Each reader returns false, and writes nothing, for a string that is not a value of
// its form.

// A keyword of an attribute, and the value that it stands for.
struct JsKitKeyword {
    PrStringView name;
    uint8_t value;
};

// Finds the keyword `text` among `keywords`.
bool jsKitKeywordFrom(Span<const JsKitKeyword> keywords, PrStringView text, uint8_t* out);

// Returns the name of `value` among `keywords`, or the empty string if none stands for it.
PrStringView jsKitKeywordName(Span<const JsKitKeyword> keywords, uint8_t value);

// Reads a decimal number that is the whole of `text`: an optional sign, digits, an optional fraction, and an optional
// exponent, as a script writes a number.
bool jsKitNumberFrom(PrStringView text, double* out);

// Read each attribute's form, or with `reset`, which is how a script removes an attribute, write the form's zero: the
// first keyword, zero, false, no edges, the auto size, and the null string. A length is a number that is zero or more.
// Several keywords are one or more of them separated by spaces, which give the union of their values, as the sides of a
// box that are ruled. A flag is true if it is present with no value, or `true`, and false if it is `false`. Edges are
// one to four lengths separated by spaces, which give the edges as CSS's shorthands do. A size is `auto`, `fill`, a
// length in px, or a percentage of the parent's size. A string is copied, and the node's kind resets it as the node is
// destroyed.
bool jsKitSetKeyword(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out);
bool jsKitSetKeywords(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out);
bool jsKitSetNumber(PrStringView value, bool reset, double* out);
bool jsKitSetFloat(PrStringView value, bool reset, float* out);
bool jsKitSetLength(PrStringView value, bool reset, float* out);
bool jsKitSetUint(PrStringView value, bool reset, uint32_t* out);
bool jsKitSetFlag(PrStringView value, bool reset, bool* out);
bool jsKitSetEdges(PrStringView value, bool reset, PrKitEdges* out);
bool jsKitSetSize(PrStringView value, bool reset, PrKitSize* out);
bool jsKitSetString(PrStringView value, bool reset, PrRawString* out);

// Reads an icon of the kit's set by its name (prKitIconName), or with `reset` writes no icon.
bool jsKitSetIcon(PrStringView value, bool reset, PrKitIconKind* out);

// Reads a color by its channels, for an attribute that takes any color rather than one of the palette by name. The
// value has one of two forms. The first is the hex value that prKitColorParseHex takes, `#RRGGBB` or `#RRGGBBAA`. The
// second is three or four numbers separated by spaces: the red, green and blue channels of extended sRGB, and the
// alpha, which is 1 if it is left out. A number may have an exponent, as a script's conversion of a small number to a
// string gives it. A channel may lie outside 0 to 1, which is how a color past sRGB is given. With `reset`, writes
// transparent black.
bool jsKitSetColor(PrStringView value, bool reset, PrColor* out);

// The keywords that the kit's kinds share: a text's role and tone, where a box puts its children or a widget sits
// across its parent's axis, and the height of a control.
Span<const JsKitKeyword> jsKitRoles();
Span<const JsKitKeyword> jsKitTones();
Span<const JsKitKeyword> jsKitAlignments();
Span<const JsKitKeyword> jsKitControlSizes();

// A color of the kit's palette, which an attribute names so that it follows the theme.
enum {
    JsKitColor_None,
    JsKitColor_Accent,
    JsKitColor_AccentWeak,
    JsKitColor_TextSelection,
    JsKitColor_AccentText,
    JsKitColor_AccentHover,
    JsKitColor_AccentPress,
    JsKitColor_Success,
    JsKitColor_Warning,
    JsKitColor_Danger,
    JsKitColor_Bg0,
    JsKitColor_Bg1,
    JsKitColor_Bg2,
    JsKitColor_Bg3,
    JsKitColor_Bg4,
    JsKitColor_BgViewport,
    JsKitColor_Line1,
    JsKitColor_Line2,
    JsKitColor_Line3,
    JsKitColor_Text1,
    JsKitColor_Text2,
    JsKitColor_Text3,
    JsKitColor_Text4,
    JsKitColor_TextInverse,
    JsKitColor_HudFill,
    JsKitColor_HudBorder,
    JsKitColor_AxisX,
    JsKitColor_AxisY,
    JsKitColor_AxisZ,
    JsKitColor_AxisW,
};
typedef uint8_t JsKitColor;

// The palette's colors by name, and the color that a name stands for in the kit's theme, which is transparent black for
// none.
Span<const JsKitKeyword> jsKitColors();
PrColor jsKitColorFrom(const PrKit& kit, JsKitColor color);

// A color that an attribute gives a widget whose content holds a PrColor: a color of the kit's palette, which follows
// the theme, or a color of its own. Zero is transparent black.
struct JsKitColorValue {
    JsKitColor named;               // JsKitColor_None if the color is `own`
    PrColor own;                    // in sRGB, with straight alpha, as the palette's colors are

    // The share of white that a named color is mixed with, from 0 to 1, or the share of black, negated. The mix is of
    // the color's red, green and blue channels in sRGB, and keeps its alpha.
    float shade;
};

// Reads a color, or with `reset` writes transparent black. A color is one of three forms. The first is the name of a
// color of the palette (jsKitColors), which is that of PrKitPalette's field with a hyphen between its words and none
// before a digit, as `accent-weak` and `bg0`. A space and a shade may follow it, a number from -1 to 1, which gives a
// tint or a shade of the color that follows the theme as the color does: `accent 0.6` is the accent with six tenths of
// white, and `accent -0.2` the accent with two tenths of black. The second is `#` and three, four, six or eight
// hexadecimal digits (css-color-4/#hex-notation). The third is three or four numbers separated by spaces, which are the
// red, green, blue and alpha channels, where 1 is a channel's full value and alpha is 1 if it is left out. Returns
// false, and writes nothing, for any other text.
bool jsKitSetColorValue(PrStringView value, bool reset, JsKitColorValue* out);

// Returns the color that `value` stands for in the kit's theme.
PrColor jsKitColorFromValue(const PrKit& kit, const JsKitColorValue& value);

// ---------------------------------------------------------------------------------------------------------------------
// Nodes

// A node's position in the tree's nodes. The root is at 0.
typedef uint32_t JsKitNodeIndex;
constexpr JsKitNodeIndex kJsKitNodeNone = 0xffffffffu;

struct JsKitKind;
struct JsKitNodeTree;

// The node's kind, and its block of the kind's data, which is null for a kind with none.
const JsKitKind* jsKitNodeKind(const JsKitNodeTree* tree, JsKitNodeIndex node);
void* PR_NULLABLE jsKitNodeData(const JsKitNodeTree* tree, JsKitNodeIndex node);

// The text that the script gave the node, valid until the script changes it.
PrStringView jsKitNodeText(const JsKitNodeTree* tree, JsKitNodeIndex node);

// The node's place among the others, each kJsKitNodeNone if there is none.
JsKitNodeIndex jsKitNodeParent(const JsKitNodeTree* tree, JsKitNodeIndex node);
JsKitNodeIndex jsKitNodeFirstChild(const JsKitNodeTree* tree, JsKitNodeIndex node);
JsKitNodeIndex jsKitNodeNextSibling(const JsKitNodeTree* tree, JsKitNodeIndex node);

// Returns true if the node's parent is of the kind named `kind`. A part that the kit creates under its container alone
// is declared only if this holds, since a script may put a node under any other.
bool jsKitNodeIsUnder(const JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView kind);

// How the script placed the node in its parent, by the attributes that every kind takes, and whether it set any.
PrKitPlace jsKitNodePlace(const JsKitNodeTree* tree, JsKitNodeIndex node);
bool jsKitNodeIsPlaced(const JsKitNodeTree* tree, JsKitNodeIndex node);

// ---------------------------------------------------------------------------------------------------------------------
// Rounds

// The round that a tree is declared in.
struct JsKitRound {
    JsKitNodeTree* tree;
    PrKit* kit;
    PrLayoutNode base;              // the host's node, which the nodes' keys are made under
};

// What an event tells the script beyond its node and its type: up to four numbers, and a text. The script reads the
// first number as the event's `value`, all of them as its `values`, and the text as its `text`.
struct JsKitDetail {
    double values[4];
    uint32_t valueCount;
    bool hasText;
    PrStringView text;              // copied as the event is queued
};

// Returns the node's key, which is the same in every round, wherever the script moves the node: a key of the node's
// own, or the key of the name that the script gave it with `key`, which two nodes of one round share only by the
// script's error. `part` tells the keys of a kind that declares several keyed nodes apart, and is 0 for its widget.
PrUiKey jsKitNodeKey(const JsKitRound& round, JsKitNodeIndex node, uint32_t part = 0);

// Returns true if the script listens for the event at index `event` of the kind's events on the node.
bool jsKitNodeListens(const JsKitNodeTree* tree, JsKitNodeIndex node, uint32_t event);

// Queues the event at index `event` of the kind's events for the script, if it listens for it on the node. A null
// `detail` tells the script nothing more.
void jsKitEventPush(const JsKitRound& round, JsKitNodeIndex node, uint32_t event, const JsKitDetail* PR_NULLABLE detail);

// Declares a node under `parent`, with the nodes under it, or only the nodes under it, in the tree's order.
void jsKitDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
void jsKitDeclareChildren(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);

// ---------------------------------------------------------------------------------------------------------------------
// Kinds

// The form of an attribute that a node's data holds as a field, each of which the reader of its name reads
// (jsKitSetKeyword, jsKitSetFlag, and so on).
enum {
    JsKitFieldType_Keyword,         // a uint8_t, one of the field's keywords
    JsKitFieldType_Keywords,        // a uint8_t, the union of one or more of the field's keywords
    JsKitFieldType_Flag,            // a bool
    JsKitFieldType_Float,           // a float
    JsKitFieldType_Length,          // a float that is zero or more
    JsKitFieldType_Uint,            // a uint32_t
    JsKitFieldType_Edges,           // a PrKitEdges
    JsKitFieldType_Size,            // a PrKitSize
    JsKitFieldType_Icon,            // a PrKitIconKind
    JsKitFieldType_String,          // a PrStringView of the text, which the data holds a copy of
    JsKitFieldType_Color,           // a PrColor, which each round gives the color that the data holds
};
typedef uint8_t JsKitFieldType;

// An attribute that a node's data holds as a field, in the type that the kit's widget takes it in. The tree sets and
// resets such an attribute itself, so a kind's `set` reads only the attributes that are not fields.
struct JsKitField {
    PrStringView name;              // the name of the attribute
    JsKitFieldType type;
    uint32_t offset;                // where the field is in the data, in bytes

    // Where the data holds the value that the field is given from, in bytes, for a type whose field does not hold it:
    // the PrRawString of a String, which the field views and the tree resets as the node is destroyed, or the
    // JsKitColorValue of a Color, which follows the kit's theme.
    uint32_t owner;

    // Keyword and Keywords: the keywords that the field takes. A reset leaves the value zero.
    const JsKitKeyword* PR_NULLABLE keywords;
    uint32_t keywordCount;
};

// An offset that names no field of a node's data.
constexpr uint32_t kJsKitOffsetNone = 0xffffffffu;

// One of the kit's queries of what a widget did in the current round, such as prKitActivated.
typedef bool JsKitQueryFn(const PrKit* kit, PrLayoutNode node);

// How jsKitWidgetDeclare declares the widget of a kind whose data holds what the kit's function takes after the kit and
// the parent: for most, a content and a style.
struct JsKitWidget {
    // Calls the kit's function with what `data` holds, and returns the widget's node.
    PrLayoutNode (*PR_NONNULL create)(PrKit* kit, PrLayoutNode parent, const void* data);

    // The offsets in the data of what each round gives the widget from its node, before it calls `create`: the
    // content's key, a PrUiKey, the string that shows the node's text, a PrStringView, and the place of a style that
    // holds one, a PrKitPlace, whose kind has JsKitKindFlag_PlacesItself. Each is kJsKitOffsetNone for a widget that
    // takes no such thing.
    uint32_t key;
    uint32_t text;
    uint32_t place;

    // The name of the kind that a node's parent must be of for the node to be declared, as a segment goes under a
    // segmented control alone, or the empty string for a kind that goes anywhere.
    PrStringView under;

    // A query for each of the kind's events, by the event's index. The kind queues the event, with no detail, in each
    // round in which its query holds for the widget. Null for a kind with no events.
    JsKitQueryFn* PR_NONNULL const* PR_NULLABLE queries;
};

// What a kind does besides declaring its widget.
enum {
    // The kind's declare applies the node's place itself, as a box does, whose style holds one. The tree places the
    // widget of any other kind with prKitPlace once it is declared, if the script placed the node.
    JsKitKindFlag_PlacesItself = 1u << 0,

    // The kind's declare reads the nodes under its node itself, as a timeline reads its tracks, or its widget holds
    // nothing, as an icon's, so the tree declares none of them under the widget that the declare returns.
    JsKitKindFlag_DeclaresChildren = 1u << 1,
};
typedef uint8_t JsKitKindFlags;

// A kind of node: the element of its name in a script's markup.
struct JsKitKind {
    PrStringView name;

    // The size of a node's data, in bytes. The data starts as zeros, so zero is every attribute's default.
    uint32_t dataSize;

    // The types of the events that the kind queues, such as `click`. A script listens for these alone.
    const PrStringView* PR_NULLABLE events;
    uint32_t eventCount;

    JsKitKindFlags flags;

    // Called with a node's data once it is zeroed, and before it is freed, for a kind whose data owns something, such
    // as a string or one of the kit's objects.
    void (*PR_NULLABLE create)(void* data);
    void (*PR_NULLABLE destroy)(void* data);

    // The attributes that the data holds as fields. The tree looks an attribute up among these before it calls `set`.
    const JsKitField* PR_NULLABLE fields;
    uint32_t fieldCount;

    // What jsKitWidgetDeclare declares the widget from. Null for a kind with a `declare` of its own.
    const JsKitWidget* PR_NULLABLE widget;

    // Sets the attribute `name` from `value`, or resets it. Returns false, leaving the data as it was, if the kind has
    // no such attribute or the value does not suit it. Null for a kind whose attributes are all fields.
    bool (*PR_NULLABLE set)(void* data, PrStringView name, PrStringView value, bool reset);

    // Declares the node's widget under `parent` in the round, and queues its events. Returns the widget's node. The
    // tree places it, applies what a node of any kind takes, and declares the node's children under it, unless the
    // kind's flags leave the placing or the children to the declare. A kind that declares no widget returns
    // kPrLayoutNodeNone, and the tree does nothing more for the node. So does a kind that declares its node's children
    // itself without JsKitKindFlag_DeclaresChildren, whose widget the tree then applies nothing to.
    PrLayoutNode (*PR_NONNULL declare)(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
};

// Returns a node's data as the kind's own type.
template <typename Data>
Data& jsKitData(const JsKitRound& round, JsKitNodeIndex node) {
    return *static_cast<Data*>(jsKitNodeData(round.tree, node));
}

// Returns the kind of `name`, or null if no family has one. A family is the kinds of one of the kit's headers that are
// written by hand, in the source named for the header, or the described kinds. Every kind's name is unique among them
// all.
const JsKitKind* PR_NULLABLE jsKitKindFrom(PrStringView name);

// The `declare` of a kind that has a JsKitWidget. It gives the widget's content and style what they take from the node,
// creates the widget, and queues each of the kind's events whose query holds. It declares nothing for a node that is
// not under the kind that the widget goes under.
PrLayoutNode jsKitWidgetDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);

// Returns the kinds that the kit's headers describe, which kit_node_quickjs_kinds.txt lists.
Span<const JsKitKind> jsKitDescribedKinds();

// ---------------------------------------------------------------------------------------------------------------------
// Trees
//
// What the binding's object, `kitTree`, edits a tree through. A node that these take exists, unless a function says
// otherwise.

// Creates a tree that holds its root alone, or frees a tree and its nodes.
JsKitNodeTree* jsKitTreeCreate();
void jsKitTreeDestroy(JsKitNodeTree* tree);

// Returns true if `node` is a node of the tree, which a destroyed node is not.
bool jsKitTreeHas(const JsKitNodeTree* tree, JsKitNodeIndex node);

// Returns true once a node was created, destroyed, moved or changed since the tree was last declared.
bool jsKitTreeIsChanged(const JsKitNodeTree* tree);

// Returns the theme that the script asked for with the root's `theme`, which is the only attribute that the root takes.
// It is the system's until the script asks.
PrJsKitTheme jsKitTreeTheme(const JsKitNodeTree* tree);

// Returns a number that grows each time the script creates, destroys, moves or changes a node. A kind that keeps what
// it read from the nodes under its own compares the number with the one that it read them at, and reads them again if
// the two differ.
uint64_t jsKitTreeVersion(const JsKitNodeTree* tree);

// Creates a detached node of `kind`, or frees a node, which is first detached, and the nodes under it. A freed node's
// index names nothing until every queued event is dispatched, so that an event never reaches a node made after it.
JsKitNodeIndex jsKitNodeCreate(JsKitNodeTree* tree, const JsKitKind* kind);
void jsKitNodeDestroy(JsKitNodeTree* tree, JsKitNodeIndex node);

// Returns true if `node` is `ancestor` or a node under it.
bool jsKitNodeIsWithin(const JsKitNodeTree* tree, JsKitNodeIndex node, JsKitNodeIndex ancestor);

// Puts a node under `parent`, before its child `before`, or last if `before` is kJsKitNodeNone. A node that has a
// parent moves. Detaches a node from its parent, if it has one.
void jsKitNodeInsert(JsKitNodeTree* tree, JsKitNodeIndex parent, JsKitNodeIndex node, JsKitNodeIndex before);
void jsKitNodeRemove(JsKitNodeTree* tree, JsKitNodeIndex node);

// Sets a node's attribute, one of those that every kind takes or one of its kind's own, or resets it. Returns false,
// and changes nothing, if the node takes no such attribute or `value` does not suit it.
bool jsKitNodeSet(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView name, PrStringView value, bool reset);

// Sets the node's text, which the tree copies.
void jsKitNodeSetText(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView text);

// Starts or stops queuing a node's events of `type`. Returns false if the node's kind queues none of that type.
bool jsKitNodeListen(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView type, bool listening);

// Queues `round` for each node under the root that the script listens for it on, with the time, the delta and the index
// of the open round of the kit's context, and returns true if it queued any. It first forgets the events that an
// earlier round queued and no one dispatched. It does nothing while the script listens for `round` on no node.
bool jsKitTreeQueueRound(JsKitNodeTree* tree, const PrKit* kit);

// Declares the tree's nodes under `parent` in the kit's open round, after forgetting the events that an earlier round
// queued and no one dispatched.
void jsKitTreeDeclare(JsKitNodeTree* tree, PrKit* kit, PrLayoutNode parent);

// An event that a round queued for the script. Its text is valid until the events are cleared.
struct JsKitQueuedEvent {
    JsKitNodeIndex node;
    PrStringView type;
    JsKitDetail detail;
};

// The events that the last round queued, in the order it queued them, and their end once they are dispatched.
uint32_t jsKitTreeEventCount(const JsKitNodeTree* tree);
JsKitQueuedEvent jsKitTreeEvent(const JsKitNodeTree* tree, uint32_t index);
void jsKitTreeClearEvents(JsKitNodeTree* tree);

} // namespace pr_impl

PR_ASSUME_NONNULL_END
