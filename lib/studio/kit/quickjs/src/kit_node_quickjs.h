// What the sources of Studio Kit's QuickJS binding share: the tree of nodes that a script edits, the kinds of node,
// each of which declares one of the kit's widgets, and the values that their attributes are read from.
//
// A kind is a description (JsKitKind) that a family's source defines, one source for each of the kit's headers. A node
// of a kind owns a block of the kind's data, which the kind's `set` writes from the attributes that the script sets and
// its `declare` reads in each round. The tree owns the node's place among the others, its text, how it sits in its
// parent, and which of its kind's events the script listens for.

#pragma once

#include "pr/base.h"
#include "pr/color.h"
#include "pr/kit.h"
#include "pr/kit_icon.h"
#include "pr/kit_icon_kind.h"
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
// A flag is true if it is present with no value, or `true`, and false if it is `false`. Edges are one to four lengths
// separated by spaces, which give the edges as CSS's shorthands do. A size is `auto`, `fill`, a length in px, or a
// percentage of the parent's size. A string is copied, and the node's kind resets it as the node is destroyed.
bool jsKitSetKeyword(Span<const JsKitKeyword> keywords, PrStringView value, bool reset, uint8_t* out);
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

// Returns the node's key, which is the same in every round, wherever the script moves the node. `part` tells the keys
// of a kind that declares several keyed nodes apart, and is 0 for its widget.
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

// What a kind does besides declaring its widget.
enum {
    // The kind's declare applies the node's place itself, as a box does, whose style holds one. The tree places the
    // widget of any other kind with prKitPlace once it is declared, if the script placed the node.
    JsKitKindFlag_PlacesItself = 1u << 0,
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

    // Sets the attribute `name` from `value`, or resets it. Returns false, leaving the data as it was, if the kind has
    // no such attribute or the value does not suit it. Null for a kind with no attributes of its own.
    bool (*PR_NULLABLE set)(void* data, PrStringView name, PrStringView value, bool reset);

    // Declares the node's widget under `parent` in the round, and queues its events. Returns the widget's node, which
    // the tree places and declares the node's children under. A kind that declares its node's children itself, or
    // declares none, returns kPrLayoutNodeNone.
    PrLayoutNode (*PR_NONNULL declare)(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
};

// Returns a node's data as the kind's own type.
template <typename Data>
Data& jsKitData(const JsKitRound& round, JsKitNodeIndex node) {
    return *static_cast<Data*>(jsKitNodeData(round.tree, node));
}

// Returns the kind of `name`, or null if no family has one. A family is the kinds of one of the kit's headers, in the
// source named for the header, and every kind's name is unique among them all.
const JsKitKind* PR_NULLABLE jsKitKindFrom(PrStringView name);

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

// Sets a node's attribute, the placement that every kind takes or one of its kind's own, or resets it. Returns false,
// and changes nothing, if the node takes no such attribute or `value` does not suit it.
bool jsKitNodeSet(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView name, PrStringView value, bool reset);

// Sets the node's text, which the tree copies.
void jsKitNodeSetText(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView text);

// Starts or stops queuing a node's events of `type`. Returns false if the node's kind queues none of that type.
bool jsKitNodeListen(JsKitNodeTree* tree, JsKitNodeIndex node, PrStringView type, bool listening);

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
