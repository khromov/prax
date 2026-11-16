// The kinds of Studio Kit's splits and docks (pr/kit_dock.h): `split`, `split-pane`, `dock` and `dock-pane`.
//
// A `split` is a box that its `split-pane` children divide by weight, with a band between each pair of panes that the
// user drags. It declares its `split-pane` children alone, each as one pane, in their order. A split with none is an
// empty box. It shows no text.
//
//   direction       `column` or `row`: the axis that the panes lie along
//   min-share       a number from 0 to 1: the smallest share of the total weight that a drag leaves to either pane
//                   beside its band
//   weights         lengths separated by spaces: the weight of each pane, in order. A pane past them weighs 1
//
//   change          a drag of a band moved weight between the two panes beside it. `text` is the new value of
//                   `weights`, with a weight for every pane. `values` are the index of the pane before the band, that
//                   pane's weight, and the weight of the pane after the band
//
// The weights stay as a drag left them until the script sets `weights`.
//
// A `split-pane` is a pane of the split that it is under. The nodes under it are the pane's content, in a column that
// clips them. It takes no attributes of its own, its placement is not read, and it queues no events.
//
// A `dock` shows its `dock-pane` children as tabbed panels in an arrangement of splits and leaves, which the user
// rearranges. It owns the kit's model of the arrangement (PrKitDock). A script never handles the model, its nodes or
// its panel ids: it gives the whole arrangement as one value, the layout, and hears the whole arrangement back. A dock
// declares its `dock-pane` children alone. It shows no text.
//
//   layout          a JSON text: the arrangement of the dock's panes, which the next paragraphs describe. With none,
//                   the dock is one empty leaf
//
//   change          the user rearranged the dock. `text` is the layout of the model as it is now, in the form that
//                   `layout` takes, with weights that sum to 1 in each split
//
// A node of a layout is a JSON object, which is a leaf or a split:
//
//   {"tabs": ["scene", "project"], "active": "project", "primary": true, "weight": 1}
//   {"direction": "row", "children": [node, node], "weight": 1}
//
//   tabs            the names of a leaf's panes, in the order of its tab strip. A leaf with none has no tabs
//   active          the name of the tab whose pane is open. The first tab is open if it is absent or names no tab
//   primary         true on the leaf that stays when its last tab leaves, where it shows that it holds no panel. If no
//                   leaf is marked, the first leaf that holds a tab is the primary leaf
//   children        a split's nodes, in order along its axis. A node that has this member is a split
//   direction       `row` or `column`: a split's axis, which every split gives
//   weight          a node's share of its parent split, relative to the weights of the split's other children. It is
//                   zero or more, and 1 if it is absent. The root's weight means nothing
//
// Other members are ignored. The dock refuses a `layout` that is not such a text.
//
// The model holds less than a layout can say, so the dock brings a layout to the nearest arrangement that the model
// holds, as the kit does when a panel leaves. A tab that names no pane under the dock is left out, and so is a second
// tab of one pane. A leaf other than the primary leaf that is left with no tabs is left out, and its weight goes to the
// child before it, or to the child after it if it was the first. A split that is left with one child gives way to that
// child, and a split inside a split along the same axis gives its children to the outer split, each with its share of
// the inner split's weight. The dock brings the model in step with the layout when the script sets `layout`, and when a
// pane arrives under the dock, leaves it, or takes another name.
//
// The user changes the model by dragging a band between two nodes, by clicking a tab, by closing a tab, and by dragging
// a tab onto another leaf's tabs, near an edge of a leaf, or near an edge of the dock. Each change stands, and the dock
// queues `change`. From then on the layout that the event told is the dock's, as if the script had set it, until the
// script sets `layout`. A script that sets `layout` to the text of a `change` changes nothing.
//
// A `dock-pane` is one panel of the dock that it is under. Its text is the title of its tab. The nodes under it are the
// panel's content, which the dock declares while the pane is the open tab of a leaf, in a column that clips them. The
// content keeps its widgets' state as the pane moves between leaves. A pane that no tab names is not shown. Its
// placement is not read.
//
//   name            a string: the name that a layout's tabs name the pane by. If two panes under a dock have one name,
//                   the tabs name the first
//   dirty           a flag: the tab marks the pane as holding unsaved changes
//
//   remove          the user closed the pane's tab, and the model holds the pane no more. The event comes before the
//                   dock's `change`. The script destroys the pane, or puts it back by setting `layout`
//
// A script reaches the rest of pr/kit_dock.h through `layout` alone: it adds, moves and removes a panel, splits a leaf,
// weighs a split's children and opens a tab by setting a layout that says so, and reads where everything is from the
// text of `change`.

#include "studio/kit/quickjs/src/kit_dock_quickjs.h"

#include "pr/assert.h"
#include "pr/bit_cast.h"
#include "pr/float.h"
#include "pr/hash.h"
#include "pr/json.h"
#include "pr/kit_box.h"
#include "pr/kit_dock.h"
#include "pr/raw_array.h"
#include "pr/raw_string.h"

namespace pr_impl {

namespace {

// The family's kinds, by their position among them.
enum {
    DockKind_Split,
    DockKind_SplitPane,
    DockKind_Dock,
    DockKind_DockPane,
    DockKind_Count,
};
typedef uint32_t DockKind;
extern const JsKitKind kKinds[DockKind_Count];

constexpr JsKitKeyword kDirections[] = {{"column", PrKitDirection_Column}, {"row", PrKitDirection_Row}};

// ---------------------------------------------------------------------------------------------------------------------
// Splits

// The events of a split, by their index.
enum { SplitEvent_Change };
constexpr PrStringView kSplitEvents[] = {"change"};

struct SplitData {
    PrKitDirection direction;
    float minShare;

    // The panes' weights, by pane, as the script set them and the drags of the bands left them. A round gives each pane
    // past them a weight of 1.
    RawArray<float> weights;

    RawArray<float> before;         // the weights that a round began with, which tell what its drag changed
    RawArray<char> text;            // the text of the `change` that a round queues
};

// Finds the part of `text` at or after `*at`, where spaces separate the parts, and moves `*at` past it. Returns false
// if no part is left.
bool nextPart(PrStringView text, size_t* at, PrStringView* part) {
    size_t start = *at;
    while (start < text.size && text.data[start] == ' ')
        ++start;
    size_t end = start;
    while (end < text.size && text.data[end] != ' ')
        ++end;
    *at = end;
    *part = PrStringView{text.data + start, end - start};
    return end > start;
}

// Sets a split's weights from lengths separated by spaces, or with `reset` to none. Returns false, leaving the weights
// as they were, if a part of `value` is not a length.
bool setWeights(PrStringView value, bool reset, RawArray<float>* out) {
    size_t at = 0;
    PrStringView part = {};
    float weight = 0;
    while (!reset && nextPart(value, &at, &part)) {
        if (!jsKitSetLength(part, false, &weight))
            return false;
    }
    out->clear();
    at = 0;
    while (!reset && nextPart(value, &at, &part)) {
        jsKitSetLength(part, false, &weight);
        out->push(weight);
    }
    return true;
}

// Appends `value` to `text` in the form that an attribute reads a number in, which has no exponent.
void appendNumber(RawArray<char>* text, float value) {
    char digits[kPrF64FixedMaxBytes];
    size_t size = prF32Format(value, digits);
    // The shortest form of a number below a millionth has an exponent.
    if (value > 0 && value < 1e-6f)
        size = prF64FormatFixed(double(value), kPrF64MaxDigits, digits);
    text->append(digits, uint32_t(size));
}

void splitDestroy(void* data) {
    SplitData& split = *static_cast<SplitData*>(data);
    split.weights.reset();
    split.before.reset();
    split.text.reset();
}

bool splitSet(void* data, PrStringView name, PrStringView value, bool reset) {
    SplitData& split = *static_cast<SplitData*>(data);
    if (name == "direction")
        return jsKitSetKeyword(kDirections, value, reset, &split.direction);
    if (name == "min-share") {
        float share = 0;
        if (!jsKitSetLength(value, reset, &share) || share > 1)
            return false;
        split.minShare = share;
        return true;
    }
    if (name == "weights")
        return setWeights(value, reset, &split.weights);
    return false;
}

// A pane of a split or of a dock declares nothing by itself: its split or its dock declares the nodes under it, in the
// box that the kit made for the pane.
PrLayoutNode paneDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

bool isSplitPane(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return jsKitNodeKind(tree, node) == &kKinds[DockKind_SplitPane];
}

PrLayoutNode splitDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    SplitData& split = jsKitData<SplitData>(round, node);
    const JsKitNodeTree* tree = round.tree;
    PrKit* kit = round.kit;
    uint32_t count = 0;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child))
        count += isSplitPane(tree, child) ? 1 : 0;
    PrUiKey key = jsKitNodeKey(round, node);
    PrKitPlace place = jsKitNodePlace(tree, node);
    if (count == 0) {
        // The kit's split has a pane at least, so a split with none is the box that the kit would divide.
        prKitBox(kit, parent, {.key = key}, {.direction = split.direction, .overflow = PrKitOverflow_Clip, .place = place});
        return kPrLayoutNodeNone;
    }
    while (split.weights.count < count)
        split.weights.push(1);
    split.before.clear();
    split.before.append(split.weights.data, count);
    float* weights = split.weights.data;
    PrLayoutNode made = prKitSplit(kit, parent, {.key = key, .count = count, .weights = weights},
        {.direction = split.direction, .minShare = split.minShare, .place = place});
    uint32_t pane = 0;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isSplitPane(tree, child))
            jsKitDeclareChildren(round, child, prKitSplitPane(kit, made, pane++));
    }

    // A drag moves weight between the two panes beside its band, so the first weight that changed is that of the pane
    // before the band.
    uint32_t dragged = 0;
    while (dragged < count && weights[dragged] == split.before.data[dragged])
        ++dragged;
    if (dragged + 1 >= count || !jsKitNodeListens(tree, node, SplitEvent_Change))
        return kPrLayoutNodeNone;
    split.text.clear();
    for (uint32_t i = 0; i < count; ++i) {
        if (i > 0)
            split.text.push(' ');
        appendNumber(&split.text, weights[i]);
    }
    JsKitDetail detail{
        .values = {double(dragged), double(weights[dragged]), double(weights[dragged + 1])},
        .valueCount = 3,
        .hasText = true,
        .text = PrStringView{split.text.data, split.text.count},
    };
    jsKitEventPush(round, node, SplitEvent_Change, &detail);
    return kPrLayoutNodeNone;
}

// ---------------------------------------------------------------------------------------------------------------------
// Layouts

// A node's position in a layout's nodes.
typedef uint32_t DockLayoutIndex;
constexpr DockLayoutIndex kDockLayoutNone = 0xffffffffu;

// The deepest that a layout's nodes nest, and the greatest weight of one.
constexpr uint32_t kLayoutMaxDepth = 64;
constexpr double kLayoutMaxWeight = 1e9;

// How near two shares of a split are for the dock to take them as one. A layout's weights are in any scale, so the
// shares that they make differ from the model's by the rounding of the division.
constexpr float kShareTolerance = 1e-5f;

// A node of a layout: a leaf, which holds tabs, or a split, which holds nodes.
struct DockLayoutNode {
    DockLayoutIndex next;           // the next child of the node's parent, or kDockLayoutNone
    DockLayoutIndex firstChild;     // a split's first child, or kDockLayoutNone
    uint32_t tabStart;              // a leaf's tabs, which are a range of the layout's
    uint32_t tabCount;
    uint32_t active;                // the index of a leaf's open tab among its tabs
    float weight;
    PrKitDockKind kind;
    PrKitDirection direction;       // a split's axis
    bool primary;                   // a leaf that the layout marks as primary
};

// A tab of a layout: the name of its pane, which is a range of the layout's names.
struct DockLayoutTab {
    uint32_t start;
    uint32_t size;
};

// An arrangement that a script gave as text, or that a dock's model holds. Its root is its first node, and a layout of
// no nodes is that of one empty leaf.
struct DockLayout {
    RawArray<DockLayoutNode> nodes;     // by DockLayoutIndex
    RawArray<DockLayoutTab> tabs;
    RawArray<char> names;
};

void layoutClear(DockLayout* layout) {
    layout->nodes.clear();
    layout->tabs.clear();
    layout->names.clear();
}

void layoutReset(DockLayout* layout) {
    layout->nodes.reset();
    layout->tabs.reset();
    layout->names.reset();
}

PrStringView layoutTabName(const DockLayout& layout, uint32_t tab) {
    return PrStringView{layout.names.data + layout.tabs.data[tab].start, layout.tabs.data[tab].size};
}

// Appends the node that the JSON value at `value` describes, with the nodes under it, to `layout`, and writes its index
// to `*made`. Returns false if the value does not describe a node, which leaves `layout` with a part of it.
bool layoutRead(const PrJson* json, PrJsonValueIndex value, uint32_t depth, DockLayout* layout, DockLayoutIndex* made) {
    if (prJsonKind(json, value) != PrJsonKind_Object || depth > kLayoutMaxDepth)
        return false;
    DockLayoutNode node{.next = kDockLayoutNone, .firstChild = kDockLayoutNone, .weight = 1};
    PrJsonValueIndex weight = prJsonMember(json, value, "weight");
    if (weight != kPrJsonMissing) {
        double given = prJsonNumber(json, weight, -1);
        if (prJsonKind(json, weight) != PrJsonKind_Number || !(given >= 0 && given <= kLayoutMaxWeight))
            return false;
        node.weight = float(given);
    }
    DockLayoutIndex index = layout->nodes.count;
    *made = index;

    PrJsonValueIndex children = prJsonMember(json, value, "children");
    if (children != kPrJsonMissing) {
        PrStringView direction = prJsonString(json, prJsonMember(json, value, "direction"));
        if (prJsonKind(json, children) != PrJsonKind_Array ||
                !jsKitKeywordFrom(kDirections, direction, &node.direction))
            return false;
        node.kind = PrKitDockKind_Split;
        layout->nodes.push(node);
        DockLayoutIndex last = kDockLayoutNone;
        for (PrJsonValueIndex at = prJsonFirst(json, children); at != kPrJsonMissing; at = prJsonAfter(json, children, at)) {
            DockLayoutIndex child = kDockLayoutNone;
            if (!layoutRead(json, at, depth + 1, layout, &child))
                return false;
            if (last == kDockLayoutNone)
                layout->nodes.data[index].firstChild = child;
            else
                layout->nodes.data[last].next = child;
            last = child;
        }
        return true;
    }

    PrJsonValueIndex tabs = prJsonMember(json, value, "tabs");
    PrJsonValueIndex active = prJsonMember(json, value, "active");
    PrJsonValueIndex primary = prJsonMember(json, value, "primary");
    if ((tabs != kPrJsonMissing && prJsonKind(json, tabs) != PrJsonKind_Array) ||
            (active != kPrJsonMissing && prJsonKind(json, active) != PrJsonKind_String))
        return false;
    if (primary != kPrJsonMissing) {
        PrJsonKind kind = prJsonKind(json, primary);
        if (kind != PrJsonKind_True && kind != PrJsonKind_False)
            return false;
        node.primary = kind == PrJsonKind_True;
    }
    node.kind = PrKitDockKind_Leaf;
    node.tabStart = layout->tabs.count;
    PrStringView open = prJsonString(json, active);
    bool opened = active == kPrJsonMissing;
    for (PrJsonValueIndex at = prJsonFirst(json, tabs); at != kPrJsonMissing; at = prJsonAfter(json, tabs, at)) {
        if (prJsonKind(json, at) != PrJsonKind_String)
            return false;
        PrStringView name = prJsonString(json, at);
        if (!opened && name == open) {
            node.active = layout->tabs.count - node.tabStart;
            opened = true;
        }
        layout->tabs.push(DockLayoutTab{.start = layout->names.count, .size = uint32_t(name.size)});
        layout->names.append(name.data, uint32_t(name.size));
    }
    node.tabCount = layout->tabs.count - node.tabStart;
    layout->nodes.push(node);
    return true;
}

// Writes the node at `index` of `layout`, with the nodes under it, as the JSON that layoutRead reads. A node other than
// the root says its weight.
void layoutWrite(const DockLayout& layout, DockLayoutIndex index, PrJsonWriter* writer) {
    const DockLayoutNode& node = layout.nodes.data[index];
    prJsonWriterBeginObject(writer);
    if (node.kind == PrKitDockKind_Split) {
        prJsonWriterKey(writer, "direction");
        prJsonWriterString(writer, jsKitKeywordName(kDirections, node.direction));
        prJsonWriterKey(writer, "children");
        prJsonWriterBeginArray(writer);
        for (DockLayoutIndex child = node.firstChild; child != kDockLayoutNone; child = layout.nodes.data[child].next)
            layoutWrite(layout, child, writer);
        prJsonWriterEndArray(writer);
    } else {
        prJsonWriterKey(writer, "tabs");
        prJsonWriterBeginArray(writer);
        for (uint32_t i = 0; i < node.tabCount; ++i)
            prJsonWriterString(writer, layoutTabName(layout, node.tabStart + i));
        prJsonWriterEndArray(writer);
        if (node.tabCount > 0) {
            prJsonWriterKey(writer, "active");
            prJsonWriterString(writer, layoutTabName(layout, node.tabStart + node.active));
        }
        if (node.primary) {
            prJsonWriterKey(writer, "primary");
            prJsonWriterBool(writer, true);
        }
    }
    if (index != 0) {
        // The shortest decimal that reads back as the weight, which the weight widened to a double does not write.
        char digits[kPrF64TextMaxBytes];
        size_t size = prF32Format(node.weight, digits);
        prJsonWriterKey(writer, "weight");
        prJsonWriterNumber(writer, prF64Parse(digits, size));
    }
    prJsonWriterEndObject(writer);
}

// ---------------------------------------------------------------------------------------------------------------------
// Docks

// The events of a dock and of a pane, by their index.
enum { DockEvent_Change };
constexpr PrStringView kDockEvents[] = {"change"};
enum { DockPaneEvent_Remove };
constexpr PrStringView kDockPaneEvents[] = {"remove"};

struct DockPaneData {
    PrRawString name;
    uint64_t nameHash;              // the hash of `name`, or zero for none
    bool dirty;
    bool held;                      // the model of the pane's dock held the pane when the dock last looked
};

// A node's position in the nodes of a resolved layout.
typedef uint32_t ResolvedIndex;
constexpr ResolvedIndex kResolvedNone = 0xffffffffu;

// A node of a resolved layout: a layout as a dock's model can hold it, with a pane for each tab. A split has two
// children or more, none of them a split along its own axis, and every leaf but the primary leaf has a tab.
struct ResolvedNode {
    ResolvedIndex next;             // the next child of the node's parent, or kResolvedNone
    ResolvedIndex firstChild;       // a split's first child
    uint32_t childCount;
    uint32_t tabStart;              // a leaf's tabs, which are a range of the resolved tabs
    uint32_t tabCount;
    uint32_t active;                // the index of a leaf's open tab among its tabs
    float weight;
    PrKitDockKind kind;
    PrKitDirection direction;       // a split's axis
    bool primary;                   // the primary leaf, or a split that holds it
};

struct DockData {
    PrKitDock* dock;

    // The arrangement that the model is kept in step with: what the script set, or the model as the user last
    // rearranged it.
    DockLayout layout;

    bool layoutSet;                 // the script set the layout since the model was brought in step with it
    uint64_t panes;                 // the hash of the panes that the model was brought in step with
    uint64_t model;                 // the hash of the model as the dock last left it

    // What bringing the model in step works in, and the writer of the text of `change`, which is made when it first
    // writes. None holds anything from round to round but its storage.
    RawArray<ResolvedNode> resolved;            // by ResolvedIndex
    RawArray<PrKitDockPanelId> resolvedTabs;
    RawArray<ResolvedIndex> children;           // the children of the splits that are being made, innermost last
    RawArray<PrKitDockNode> made;               // the model's node for each of `children`
    RawArray<float> weights;
    PrJsonWriter* PR_NULLABLE writer;
};

void dockCreate(void* data) {
    static_cast<DockData*>(data)->dock = prKitDockCreate();
}

void dockDestroy(void* data) {
    DockData& dock = *static_cast<DockData*>(data);
    prKitDockDestroy(dock.dock);
    layoutReset(&dock.layout);
    dock.resolved.reset();
    dock.resolvedTabs.reset();
    dock.children.reset();
    dock.made.reset();
    dock.weights.reset();
    prJsonWriterDestroy(dock.writer);
}

bool dockSet(void* data, PrStringView name, PrStringView value, bool reset) {
    DockData& dock = *static_cast<DockData*>(data);
    if (name != "layout")
        return false;
    DockLayout layout{};
    if (!reset) {
        PrJson* json = prJsonCreate();
        DockLayoutIndex root = kDockLayoutNone;
        bool read = prJsonParse(json, value, nullptr) && layoutRead(json, 0, 0, &layout, &root);
        prJsonDestroy(json);
        if (!read) {
            layoutReset(&layout);
            return false;
        }
    }
    layoutReset(&dock.layout);
    dock.layout = layout;
    dock.layoutSet = true;
    return true;
}

void dockPaneDestroy(void* data) {
    prRawStringReset(&static_cast<DockPaneData*>(data)->name);
}

bool dockPaneSet(void* data, PrStringView name, PrStringView value, bool reset) {
    DockPaneData& pane = *static_cast<DockPaneData*>(data);
    if (name == "name") {
        jsKitSetString(value, reset, &pane.name);
        pane.nameHash = reset ? 0 : prHashBytes(value.data, value.size, 1);
        return true;
    }
    if (name == "dirty")
        return jsKitSetFlag(value, reset, &pane.dirty);
    return false;
}

bool isDockPane(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return jsKitNodeKind(tree, node) == &kKinds[DockKind_DockPane];
}

DockPaneData& dockPaneData(const JsKitNodeTree* tree, JsKitNodeIndex pane) {
    return *static_cast<DockPaneData*>(jsKitNodeData(tree, pane));
}

// Returns the first pane under `dock` that is named `name`, or kJsKitNodeNone if there is none.
JsKitNodeIndex dockPaneFromName(const JsKitNodeTree* tree, JsKitNodeIndex dock, PrStringView name) {
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, dock); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isDockPane(tree, child) && prRawStringView(&dockPaneData(tree, child).name) == name)
            return child;
    }
    return kJsKitNodeNone;
}

// Returns the hash of the panes under `dock` and their names, which changes when a layout's tabs may name other panes
// than they did.
uint64_t dockPanesHash(const JsKitNodeTree* tree, JsKitNodeIndex dock) {
    uint64_t hash = 1;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, dock); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isDockPane(tree, child))
            hash = prHashMixKeys(prHashMixKeys(hash, child), dockPaneData(tree, child).nameHash);
    }
    return hash;
}

// Returns the hash of the model's node `node` and the nodes under it, mixed with `seed`: everything that a layout says
// of them.
uint64_t dockModelHash(const PrKitDock* dock, PrKitDockNode node, uint64_t seed) {
    if (prKitDockNodeKind(dock, node) == PrKitDockKind_Leaf) {
        uint32_t count = prKitDockLeafTabCount(dock, node);
        uint64_t hash = prHashMixKeys(seed, (uint64_t(count) << 32) | prKitDockLeafActiveTab(dock, node));
        for (uint32_t i = 0; i < count; ++i)
            hash = prHashMixKeys(hash, prKitDockLeafTab(dock, node, i));
        return hash;
    }
    uint32_t count = prKitDockSplitChildCount(dock, node);
    uint64_t hash = prHashMixKeys(seed, (uint64_t(count) << 32) | (1u << 8) | prKitDockSplitDirection(dock, node));
    for (uint32_t i = 0; i < count; ++i) {
        hash = prHashMixKeys(hash, bitCast<uint32_t>(prKitDockSplitWeight(dock, node, i)));
        hash = dockModelHash(dock, prKitDockSplitChild(dock, node, i), hash);
    }
    return hash;
}

// What resolving a layout reads, and where it stands.
struct Resolution {
    const JsKitNodeTree* tree;
    JsKitNodeIndex dock;
    DockData* data;
    DockLayoutIndex primary;        // the leaf that the layout marks as primary, or kDockLayoutNone if it marks none
    bool chosen;                    // a leaf that the layout does not mark became primary
};

// Appends the resolved form of the layout's node `index`, with the nodes under it, to the dock's resolved nodes and
// tabs. Returns its index, or kResolvedNone if the model holds nothing for the node. The caller gives the node its
// weight.
ResolvedIndex dockResolve(Resolution& resolution, DockLayoutIndex index) {
    DockData& data = *resolution.data;
    const DockLayoutNode& from = data.layout.nodes.data[index];
    ResolvedNode node{
        .next = kResolvedNone,
        .firstChild = kResolvedNone,
        .kind = from.kind,
        .direction = from.direction,
    };
    if (from.kind == PrKitDockKind_Leaf) {
        node.tabStart = data.resolvedTabs.count;
        for (uint32_t i = 0; i < from.tabCount; ++i) {
            PrStringView name = layoutTabName(data.layout, from.tabStart + i);
            JsKitNodeIndex pane = dockPaneFromName(resolution.tree, resolution.dock, name);
            if (name.size == 0 || pane == kJsKitNodeNone || data.resolvedTabs.find(pane) != kArrayMissing)
                continue;
            if (i == from.active)
                node.active = data.resolvedTabs.count - node.tabStart;
            data.resolvedTabs.push(pane);
        }
        node.tabCount = data.resolvedTabs.count - node.tabStart;
        node.primary = index == resolution.primary ||
            (resolution.primary == kDockLayoutNone && !resolution.chosen && node.tabCount > 0);
        if (node.tabCount == 0 && !node.primary)
            return kResolvedNone;
        resolution.chosen = resolution.chosen || node.primary;
        data.resolved.push(node);
        return data.resolved.count - 1;
    }

    // The weight of a child that is left out, which the next child takes if no child came before it.
    float carried = 0;
    ResolvedIndex last = kResolvedNone;
    for (DockLayoutIndex child = from.firstChild; child != kDockLayoutNone; child = data.layout.nodes.data[child].next) {
        float weight = data.layout.nodes.data[child].weight;
        ResolvedIndex made = dockResolve(resolution, child);
        RawArray<ResolvedNode>& nodes = data.resolved;
        if (made == kResolvedNone) {
            if (last != kResolvedNone)
                nodes.data[last].weight += weight;
            else
                carried += weight;
            continue;
        }
        weight += carried;
        carried = 0;
        ResolvedIndex first = made;
        ResolvedIndex end = made;
        uint32_t count = 1;
        const ResolvedNode inner = nodes.data[made];
        if (inner.kind == PrKitDockKind_Split && inner.direction == from.direction) {
            // A split along the same axis gives its children to this one, each with its share of the split's weight.
            float total = 0;
            for (ResolvedIndex at = inner.firstChild; at != kResolvedNone; at = nodes.data[at].next)
                total += nodes.data[at].weight;
            for (ResolvedIndex at = inner.firstChild; at != kResolvedNone; at = nodes.data[at].next) {
                float share = total > 0 ? nodes.data[at].weight / total : 1 / float(inner.childCount);
                nodes.data[at].weight = weight * share;
                end = at;
            }
            first = inner.firstChild;
            count = inner.childCount;
        } else {
            nodes.data[made].weight = weight;
        }
        if (last == kResolvedNone)
            node.firstChild = first;
        else
            nodes.data[last].next = first;
        last = end;
        node.childCount += count;
        node.primary = node.primary || inner.primary;
    }
    if (node.childCount == 0)
        return kResolvedNone;
    // A split of one child gives way to the child.
    if (node.childCount == 1)
        return node.firstChild;
    data.resolved.push(node);
    return data.resolved.count - 1;
}

// Returns true if the model's node `node`, with the nodes under it, is arranged as the resolved node `index` says: the
// same splits and leaves, with the same tabs. Weights and open tabs may differ.
bool dockModelMatches(const DockData& data, PrKitDockNode node, ResolvedIndex index) {
    const PrKitDock* dock = data.dock;
    const ResolvedNode& to = data.resolved.data[index];
    if (prKitDockNodeKind(dock, node) != to.kind)
        return false;
    if (to.kind == PrKitDockKind_Leaf) {
        if (prKitDockLeafTabCount(dock, node) != to.tabCount || (prKitDockPrimary(dock) == node) != to.primary)
            return false;
        for (uint32_t i = 0; i < to.tabCount; ++i) {
            if (prKitDockLeafTab(dock, node, i) != data.resolvedTabs.data[to.tabStart + i])
                return false;
        }
        return true;
    }
    if (prKitDockSplitDirection(dock, node) != to.direction || prKitDockSplitChildCount(dock, node) != to.childCount)
        return false;
    uint32_t at = 0;
    for (ResolvedIndex child = to.firstChild; child != kResolvedNone; child = data.resolved.data[child].next) {
        if (!dockModelMatches(data, prKitDockSplitChild(dock, node, at++), child))
            return false;
    }
    return true;
}

// Returns a panel of the leaf that the model makes first of the resolved node `index` and the nodes under it: the first
// leaf, each of whose tabs has a panel. The node does not hold the primary leaf, which the model has from the start.
PrKitDockPanelId dockSeedPanel(const DockData& data, ResolvedIndex index) {
    while (data.resolved.data[index].kind == PrKitDockKind_Split)
        index = data.resolved.data[index].firstChild;
    return data.resolvedTabs.data[data.resolved.data[index].tabStart];
}

// Makes the resolved node `index`, with the nodes under it, of the model's leaf `leaf`. The leaf holds the first tab of
// the node's first leaf alone, or nothing if the node holds the primary leaf, which `leaf` then is.
void dockConstruct(DockData& data, ResolvedIndex index, PrKitDockNode leaf) {
    PrKitDock* dock = data.dock;
    const ResolvedNode node = data.resolved.data[index];
    if (node.kind == PrKitDockKind_Leaf) {
        for (uint32_t i = prKitDockLeafTabCount(dock, leaf); i < node.tabCount; ++i)
            prKitDockAddPanel(dock, leaf, data.resolvedTabs.data[node.tabStart + i]);
        return;
    }

    // Each child begins as a leaf beside the one that was made before it. The child that holds the primary leaf is
    // `leaf` itself, since no other leaf can become primary, so the children on either side of it are made outward from
    // it. A leaf that is split along its parent's axis joins the parent, which makes the leaves siblings.
    bool row = node.direction == PrKitDirection_Row;
    PrKitDockEdge before = row ? PrKitDockEdge_Left : PrKitDockEdge_Top;
    PrKitDockEdge after = row ? PrKitDockEdge_Right : PrKitDockEdge_Bottom;
    uint32_t base = data.children.count;
    uint32_t held = 0;
    for (ResolvedIndex child = node.firstChild; child != kResolvedNone; child = data.resolved.data[child].next) {
        if (data.resolved.data[child].primary)
            held = data.children.count - base;
        data.children.push(child);
    }
    data.made.resize(base + node.childCount);
    data.made.data[base + held] = leaf;
    for (uint32_t i = held + 1; i < node.childCount; ++i) {
        PrKitDockPanelId seed = dockSeedPanel(data, data.children.data[base + i]);
        data.made.data[base + i] = prKitDockSplit(dock, data.made.data[base + i - 1], after, seed, 0.5f);
    }
    for (uint32_t i = held; i-- > 0;) {
        PrKitDockPanelId seed = dockSeedPanel(data, data.children.data[base + i]);
        data.made.data[base + i] = prKitDockSplit(dock, data.made.data[base + i + 1], before, seed, 0.5f);
    }
    for (uint32_t i = 0; i < node.childCount; ++i)
        dockConstruct(data, data.children.data[base + i], data.made.data[base + i]);
    data.children.resize(base);
    data.made.resize(base);
}

// Gives the model's node `node`, and the nodes under it, the weights and the open tabs of the resolved node `index`,
// which the model matches (dockModelMatches).
void dockApply(DockData& data, PrKitDockNode node, ResolvedIndex index) {
    PrKitDock* dock = data.dock;
    const ResolvedNode& to = data.resolved.data[index];
    if (to.kind == PrKitDockKind_Leaf) {
        if (to.tabCount > 0 && prKitDockLeafActiveTab(dock, node) != to.active)
            prKitDockLeafSetActiveTab(dock, node, to.active);
        return;
    }
    data.weights.clear();
    float total = 0;
    for (ResolvedIndex child = to.firstChild; child != kResolvedNone; child = data.resolved.data[child].next) {
        data.weights.push(data.resolved.data[child].weight);
        total += data.resolved.data[child].weight;
    }
    bool differs = false;
    for (uint32_t i = 0; i < to.childCount; ++i) {
        float share = total > 0 ? data.weights.data[i] / total : 1 / float(to.childCount);
        differs = differs || prF32Abs(share - prKitDockSplitWeight(dock, node, i)) > kShareTolerance;
    }
    if (differs)
        prKitDockSplitSetWeights(dock, node, data.weights.data, to.childCount);
    uint32_t at = 0;
    for (ResolvedIndex child = to.firstChild; child != kResolvedNone; child = data.resolved.data[child].next)
        dockApply(data, prKitDockSplitChild(dock, node, at++), child);
}

// Notes for each pane under `dock` whether the model holds it. With a `round`, queues `remove` for a pane that the
// model held when the dock last looked and holds no more, which is one whose tab the user closed.
void dockNotePanes(const JsKitRound* PR_NULLABLE round, const JsKitNodeTree* tree, JsKitNodeIndex dock, const DockData& data) {
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, dock); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (!isDockPane(tree, child))
            continue;
        DockPaneData& pane = dockPaneData(tree, child);
        bool held = prKitDockPanelLeaf(data.dock, child) != kPrKitDockNodeNone;
        if (round != nullptr && pane.held && !held)
            jsKitEventPush(*round, child, DockPaneEvent_Remove, nullptr);
        pane.held = held;
    }
}

// Brings the model in step with the dock's layout and the panes under the dock: the model becomes the layout's resolved
// form. A model that is arranged as that form says keeps its nodes, and so the rearrangements that the kit has staged
// against them.
void dockSync(const JsKitNodeTree* tree, JsKitNodeIndex dock, DockData& data) {
    data.resolved.clear();
    data.resolvedTabs.clear();
    Resolution resolution{.tree = tree, .dock = dock, .data = &data, .primary = kDockLayoutNone};
    for (DockLayoutIndex i = 0; i < data.layout.nodes.count && resolution.primary == kDockLayoutNone; ++i) {
        if (data.layout.nodes.data[i].kind == PrKitDockKind_Leaf && data.layout.nodes.data[i].primary)
            resolution.primary = i;
    }
    ResolvedIndex root = data.layout.nodes.count > 0 ? dockResolve(resolution, 0) : kResolvedNone;
    PrKitDockNode held = prKitDockRoot(data.dock);
    bool matches = false;
    if (root != kResolvedNone)
        matches = dockModelMatches(data, held, root);
    else
        matches = prKitDockNodeKind(data.dock, held) == PrKitDockKind_Leaf && prKitDockLeafTabCount(data.dock, held) == 0;
    if (!matches) {
        prKitDockDestroy(data.dock);
        data.dock = prKitDockCreate();
        if (root != kResolvedNone)
            dockConstruct(data, root, prKitDockPrimary(data.dock));
    }
    if (root != kResolvedNone) {
        PR_ASSERT(dockModelMatches(data, prKitDockRoot(data.dock), root));
        dockApply(data, prKitDockRoot(data.dock), root);
    }
    dockNotePanes(nullptr, tree, dock, data);
}

// Appends the model's node `node`, with the nodes under it, to `layout`, with `weight` as its weight. Returns its
// index.
DockLayoutIndex dockSnapshot(const JsKitNodeTree* tree, const PrKitDock* dock, PrKitDockNode node, float weight,
        DockLayout* layout) {
    DockLayoutIndex index = layout->nodes.count;
    DockLayoutNode made{.next = kDockLayoutNone, .firstChild = kDockLayoutNone, .weight = weight};
    made.kind = prKitDockNodeKind(dock, node);
    if (made.kind == PrKitDockKind_Leaf) {
        made.tabStart = layout->tabs.count;
        made.tabCount = prKitDockLeafTabCount(dock, node);
        made.active = prKitDockLeafActiveTab(dock, node);
        made.primary = prKitDockPrimary(dock) == node;
        for (uint32_t i = 0; i < made.tabCount; ++i) {
            // The model holds panes alone, each of which the dock put there by its name.
            PrStringView name =
                prRawStringView(&dockPaneData(tree, JsKitNodeIndex(prKitDockLeafTab(dock, node, i))).name);
            layout->tabs.push(DockLayoutTab{.start = layout->names.count, .size = uint32_t(name.size)});
            layout->names.append(name.data, uint32_t(name.size));
        }
        layout->nodes.push(made);
        return index;
    }
    made.direction = prKitDockSplitDirection(dock, node);
    layout->nodes.push(made);
    DockLayoutIndex last = kDockLayoutNone;
    uint32_t count = prKitDockSplitChildCount(dock, node);
    for (uint32_t i = 0; i < count; ++i) {
        DockLayoutIndex child = dockSnapshot(tree, dock, prKitDockSplitChild(dock, node, i),
            prKitDockSplitWeight(dock, node, i), layout);
        if (last == kDockLayoutNone)
            layout->nodes.data[index].firstChild = child;
        else
            layout->nodes.data[last].next = child;
        last = child;
    }
    return index;
}

// Returns what the tab of `panel` shows: the text and the flag of the pane that the panel is. The kit also asks for a
// tab that is dragged from another dock, whose panel may be no node of this tree.
PrKitDockTab dockDescribe(void* PR_NULLABLE context, PrKitDockPanelId panel) {
    const JsKitNodeTree* tree = static_cast<const JsKitNodeTree*>(context);
    JsKitNodeIndex pane = JsKitNodeIndex(panel);
    if (panel != pane || !jsKitTreeHas(tree, pane) || !isDockPane(tree, pane))
        return PrKitDockTab{};
    return PrKitDockTab{.title = jsKitNodeText(tree, pane), .dirty = dockPaneData(tree, pane).dirty};
}

PrLayoutNode dockDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    DockData& data = jsKitData<DockData>(round, node);
    JsKitNodeTree* tree = round.tree;
    uint64_t panes = dockPanesHash(tree, node);
    if (data.layoutSet || panes != data.panes) {
        dockSync(tree, node, data);
        data.layoutSet = false;
        data.panes = panes;
        data.model = dockModelHash(data.dock, prKitDockRoot(data.dock), 0);
    }

    PrKitDockView view{.key = jsKitNodeKey(round, node), .dock = data.dock, .describe = dockDescribe, .context = tree};
    PrLayoutNode area = prKitDockBuild(round.kit, parent, view);
    if (jsKitNodeIsPlaced(tree, node))
        prKitPlace(round.kit, area, jsKitNodePlace(tree, node));
    uint32_t count = prKitDockPaneCount(data.dock);
    for (uint32_t i = 0; i < count; ++i) {
        PrKitDockPane pane = prKitDockPane(data.dock, i);
        jsKitDeclareChildren(round, JsKitNodeIndex(pane.panel), pane.content);
    }

    // The build applied what the user did to the model, which is the dock's layout from now on.
    uint64_t model = dockModelHash(data.dock, prKitDockRoot(data.dock), 0);
    if (model == data.model)
        return kPrLayoutNodeNone;
    data.model = model;
    dockNotePanes(&round, tree, node, data);
    layoutClear(&data.layout);
    dockSnapshot(tree, data.dock, prKitDockRoot(data.dock), 1, &data.layout);
    if (!jsKitNodeListens(tree, node, DockEvent_Change))
        return kPrLayoutNodeNone;
    if (data.writer == nullptr)
        data.writer = prJsonWriterCreate();
    prJsonWriterClear(data.writer);
    layoutWrite(data.layout, 0, data.writer);
    JsKitDetail detail{.hasText = true, .text = prJsonWriterText(data.writer)};
    jsKitEventPush(round, node, DockEvent_Change, &detail);
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[DockKind_Count] = {
    {
        .name = "split",
        .dataSize = sizeof(SplitData),
        .events = kSplitEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .destroy = splitDestroy,
        .set = splitSet,
        .declare = splitDeclare,
    },
    {.name = "split-pane", .declare = paneDeclare},
    {
        .name = "dock",
        .dataSize = sizeof(DockData),
        .events = kDockEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .create = dockCreate,
        .destroy = dockDestroy,
        .set = dockSet,
        .declare = dockDeclare,
    },
    {
        .name = "dock-pane",
        .dataSize = sizeof(DockPaneData),
        .events = kDockPaneEvents,
        .eventCount = 1,
        .destroy = dockPaneDestroy,
        .set = dockPaneSet,
        .declare = paneDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitDockKinds() {
    return kKinds;
}

} // namespace pr_impl
