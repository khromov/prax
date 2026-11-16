// The kinds of Studio Kit's choices and tabs (pr/kit_choice.h) that are written by hand. The header's other kinds are
// described (kit_node_quickjs_kinds.txt): the segmented control and the radio group with their items, a tab's label,
// and the progress bar, the meter and the spinner. An attribute sets the field of its name in the content or the style
// of the kind's widget, which the header describes, unless it is described here. One that is given here with an
// enumeration takes the names of its members as keywords, in lower case with a hyphen between words, and a reset leaves
// the first of them.
//
//   checkbox      Shows its node's text as its label. `value`, a flag that is the state it shows, `size` and
//                 `disabled`. Queues `change`.
//   toggle        A switch, which shows, takes and queues what a checkbox does.
//   tab-strip     `size`, `group`, a whole number, `reorderable`, `collapses-single`, `disabled`, which disables every
//                 tab, and `selected`, a tab's id, `next` or `previous`. Holds `tab` nodes. Queues `select`, `drop` and
//                 `tearoff`.
//   tab           Under a `tab-strip` alone. `id`, a whole number above 0, and `disabled`. Holds what the tab shows, in
//                 the tree's order: a `tab-label`, a `tab-close`, or nodes of any other kind. Queues `remove`.
//   tab-close     Under a `tab` alone. The tab's close control, which only the pointer uses.
//
// A node that is not under the kind it goes under is not declared, and neither are the nodes under it.
//
// Checkboxes and toggles. An activation flips `value` and queues `change`, whose `value` is the new state, 1 or 0. The
// state stays as the event says until the script sets `value`.
//
// Tabs. A tab's `id` is unique in its strip, and among the strips of its `group` if the strip has one. A tab with no
// id, or with the id of a tab before it in the strip, is not declared. Reorderable strips of one group exchange the
// tabs that the user drags between them.
//
// The strip shows its tabs in an order of its own, which starts as the tree's. A tab that the script adds goes after
// the tab that its node follows. The user's drags reorder the strip without moving a node, and the strip keeps their
// order until the script changes the order of the `tab` nodes that it had, which the strip then takes. The strip
// selects its first tab until the script or the user selects another, and a neighbor of a selected tab that the script
// removes. A `selected` of `next` or `previous` selects, in the next round, the tab after or before the selected one in
// the strip's order, around its ends (prKitTabsStep). A strip queues:
//
//   select    The strip selected a tab that `selected` did not name: the user chose it, the strip chose it as its tabs
//             changed, or `selected` asked for `next`, for `previous`, or for a tab that the strip does not hold. Its
//             `value` is the tab's id, or 0 if the strip holds no tabs.
//   drop      The user dropped a tab on the strip. Its `values` are the tab's id and its index in the strip's order,
//             counted without the tab. A tab of the strip is already there, and selected. A tab of another strip of the
//             group stays in that strip until the script moves its node under this one, which then selects it.
//   tearoff   The user dropped a tab of the strip outside every window. Its `values` are the tab's id and the x and y
//             of the drop on its display, in the display's px, and its `text` is the display's id in decimal digits,
//             which is 0 if the platform reports none. The tab stays in the strip until the script moves its node.
//
// A tab queues `remove`, with its id as the `value`, when the `tab-close` under it is activated. The tab stays until
// the script destroys its node.

#include "studio/kit/quickjs/src/kit_choice_quickjs.h"

#include "pr/format.h"
#include "pr/kit_choice.h"
#include "pr/raw_array.h"

namespace pr_impl {

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Choices

// A checkbox or a toggle. The kit flips `value` through the pointer that the node's widget is given.
struct CheckData {
    PrKitChoiceStyle style;
    bool value;
    bool disabled;
};

// The events of a checkbox and of a toggle, by their index.
enum { CheckEvent_Change };
constexpr PrStringView kCheckEvents[] = {"change"};

bool checkSet(void* data, PrStringView name, PrStringView value, bool reset) {
    CheckData& check = *static_cast<CheckData*>(data);
    if (name == "value")
        return jsKitSetFlag(value, reset, &check.value);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &check.style.size);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &check.disabled);
    return false;
}

// Declares a checkbox or a toggle with `widget`, the kit's function that creates one, and queues the flip of its value.
PrLayoutNode checkDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent,
        PrLayoutNode (*widget)(PrKit* kit, PrLayoutNode under, PrKitCheck check, PrKitChoiceStyle style)) {
    CheckData& check = jsKitData<CheckData>(round, node);
    bool before = check.value;
    PrLayoutNode made = widget(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .value = &check.value,
        .label = jsKitNodeText(round.tree, node),
        .disabled = check.disabled,
    }, check.style);
    if (check.value != before) {
        JsKitDetail detail{.values = {check.value ? 1.0 : 0.0}, .valueCount = 1};
        jsKitEventPush(round, node, CheckEvent_Change, &detail);
    }
    return made;
}

PrLayoutNode checkboxDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return checkDeclare(round, node, parent, prKitCheckbox);
}

PrLayoutNode toggleDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return checkDeclare(round, node, parent, prKitToggle);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tabs

// A tab strip. The node owns the kit's tabs, and keeps them in step with the `tab` nodes under it.
struct TabStripData {
    PrKitTabs* tabs;

    // The id of each `tab` node under the strip, in the tree's order, as of the last round that brought the kit's tabs
    // in step with them. A node with no id has 0.
    RawArray<uint64_t> ids;

    PrKitChoiceStyle style;
    uint64_t group;
    uint64_t selected;              // the selected tab as the script last set it or was last told, or 0
    bool selects;                   // the script set `selected` since the strip was last declared
    int8_t step;                    // 1 or -1 if it set `next` or `previous`, which the next round steps by, or 0

    // A tab of another strip that the user dropped on this one, or 0. It is selected once the script moves its node
    // under the strip, as the kit selects a tab that it moves (prKitTabsMove).
    uint64_t arriving;

    bool reorderable;
    bool collapsesSingle;
    bool disabled;
};

struct TabData {
    uint64_t id;                    // 0 until the script sets one
    bool disabled;
};

// The events of a tab strip and of a tab, by their index.
enum { TabStripEvent_Select, TabStripEvent_Drop, TabStripEvent_Tearoff };
constexpr PrStringView kTabStripEvents[] = {"select", "drop", "tearoff"};
enum { TabEvent_Remove };
constexpr PrStringView kTabEvents[] = {"remove"};

PrLayoutNode tabStripDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode tabDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);

void tabStripCreate(void* data) {
    static_cast<TabStripData*>(data)->tabs = prKitTabsCreate();
}

void tabStripDestroy(void* data) {
    TabStripData& strip = *static_cast<TabStripData*>(data);
    prKitTabsDestroy(strip.tabs);
    strip.ids.reset();
}

// Reads a whole number that names a tab or a group of strips, or with `reset` writes 0, which names none.
bool setId(PrStringView value, bool reset, uint64_t* out) {
    uint32_t id = 0;
    if (!jsKitSetUint(value, reset, &id))
        return false;
    *out = id;
    return true;
}

bool tabStripSet(void* data, PrStringView name, PrStringView value, bool reset) {
    TabStripData& strip = *static_cast<TabStripData*>(data);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &strip.style.size);
    if (name == "group")
        return setId(value, reset, &strip.group);
    if (name == "reorderable")
        return jsKitSetFlag(value, reset, &strip.reorderable);
    if (name == "collapses-single")
        return jsKitSetFlag(value, reset, &strip.collapsesSingle);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &strip.disabled);
    if (name == "selected") {
        bool forward = !reset && value == "next";
        bool back = !reset && value == "previous";
        if (!forward && !back && !setId(value, reset, &strip.selected))
            return false;
        strip.selects = true;
        strip.step = int8_t(int(forward) - int(back));
        return true;
    }
    return false;
}

bool tabSet(void* data, PrStringView name, PrStringView value, bool reset) {
    TabData& tab = *static_cast<TabData*>(data);
    if (name == "id")
        return setId(value, reset, &tab.id);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &tab.disabled);
    return false;
}

bool isTab(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return jsKitNodeKind(tree, node)->declare == tabDeclare;
}

uint64_t tabId(const JsKitNodeTree* tree, JsKitNodeIndex tab) {
    return static_cast<const TabData*>(jsKitNodeData(tree, tab))->id;
}

// Returns true if a strip holds the tab at `index` of `ids`: a tab with an id that no tab before it has.
bool tabIsHeld(const RawArray<uint64_t>& ids, uint32_t index) {
    return ids.data[index] != 0 && ids.find(ids.data[index]) == index;
}

// Returns true if the `tab` nodes under the strip have the ids of `strip.ids`, in its order.
bool tabsAreInStep(const JsKitNodeTree* tree, JsKitNodeIndex node, const TabStripData& strip) {
    uint32_t count = 0;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (!isTab(tree, child))
            continue;
        if (count == strip.ids.count || strip.ids.data[count] != tabId(tree, child))
            return false;
        ++count;
    }
    return count == strip.ids.count;
}

// Brings the kit's tabs in step with the `tab` nodes under the strip, which the script changed. The kit's tabs lose the
// tabs whose nodes are gone and gain the new ones, each after the tab that its node follows. They keep their own order,
// which the user's drags gave them, unless the script changed the order of the nodes that were there before. They take
// the nodes' order then.
void tabsSync(const JsKitNodeTree* tree, JsKitNodeIndex node, TabStripData& strip) {
    PrKitTabs* tabs = strip.tabs;
    RawArray<uint64_t> ids;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isTab(tree, child))
            ids.push(tabId(tree, child));
    }

    // The script reordered the nodes if two tabs that the strip held, and holds, changed places.
    bool reordered = false;
    uint32_t after = 0;
    for (uint32_t i = 0; i < ids.count; ++i) {
        uint32_t was = tabIsHeld(ids, i) ? strip.ids.find(ids.data[i]) : kArrayMissing;
        if (was == kArrayMissing)
            continue;
        reordered = reordered || was < after;
        after = was + 1;
    }

    for (uint32_t i = prKitTabsCount(tabs); i-- > 0;) {
        uint64_t id = prKitTabsAt(tabs, i);
        if (ids.find(id) == kArrayMissing)
            prKitTabsRemove(tabs, id);
    }
    // A move selects the tab that it moves, so the selection is put back afterward.
    uint64_t selected = prKitTabsSelected(tabs);
    uint64_t previous = 0;
    uint32_t held = 0;
    for (uint32_t i = 0; i < ids.count; ++i) {
        if (!tabIsHeld(ids, i))
            continue;
        uint64_t id = ids.data[i];
        uint32_t at = prKitTabsIndex(tabs, id);
        if (at == prKitTabsCount(tabs)) {
            uint32_t next = previous != 0 ? prKitTabsIndex(tabs, previous) + 1 : 0;
            prKitTabsInsert(tabs, id, reordered ? held : next);
        } else if (reordered && at != held) {
            prKitTabsMove(tabs, tabs, id, held);
        }
        previous = id;
        ++held;
    }
    if (strip.arriving != 0 && prKitTabsIndex(tabs, strip.arriving) < prKitTabsCount(tabs))
        selected = strip.arriving;
    if (selected != 0)
        prKitTabsSelect(tabs, selected);
    strip.arriving = 0;
    strip.ids.reset();
    strip.ids = ids;
}

// Queues the tab that the user dropped on the strip, or tore off it, in the round.
void tabStripQueueMove(const JsKitRound& round, JsKitNodeIndex node, TabStripData& strip, PrLayoutNode made) {
    PrKitTabMove move{};
    if (!prKitTabMoved(round.kit, made, &move)) {
        // The kit carries out a move within the strip, and reports none. The tab is then where it was dropped.
        const PrUiPayload* dropped = strip.reorderable ? prUiDropped(round.kit->ui, made) : nullptr;
        uint32_t index = dropped != nullptr ? prKitTabsIndex(strip.tabs, dropped->value) : 0;
        if (dropped == nullptr || index == prKitTabsCount(strip.tabs))
            return;
        JsKitDetail detail{.values = {double(dropped->value), double(index)}, .valueCount = 2};
        jsKitEventPush(round, node, TabStripEvent_Drop, &detail);
        return;
    }
    if (move.kind == PrKitTabMoveKind_Received) {
        strip.arriving = move.id;
        JsKitDetail detail{.values = {double(move.id), double(move.index)}, .valueCount = 2};
        jsKitEventPush(round, node, TabStripEvent_Drop, &detail);
        return;
    }
    char digits[24];
    JsKitDetail detail{
        .values = {double(move.id), double(move.position.position.x), double(move.position.position.y)},
        .valueCount = 3,
        .hasText = true,
        .text = formatToBuffer(Span(digits), "{}", move.position.display),
    };
    jsKitEventPush(round, node, TabStripEvent_Tearoff, &detail);
}

PrLayoutNode tabStripDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    TabStripData& strip = jsKitData<TabStripData>(round, node);
    PrKit* kit = round.kit;
    if (!tabsAreInStep(round.tree, node, strip))
        tabsSync(round.tree, node, strip);
    if (strip.selects && strip.step != 0) {
        // The script does not know which tab a step selects, so the strip forgets the one it knew and tells it.
        prKitTabsStep(strip.tabs, strip.step);
        strip.selected = 0;
    } else if (strip.selects && prKitTabsIndex(strip.tabs, strip.selected) < prKitTabsCount(strip.tabs)) {
        prKitTabsSelect(strip.tabs, strip.selected);
    }
    strip.selects = false;
    strip.step = 0;
    PrLayoutNode made = prKitTabStrip(kit, parent, {
        .key = jsKitNodeKey(round, node),
        .tabs = strip.tabs,
        .group = strip.group,
        .reorderable = strip.reorderable,
        .collapsesSingle = strip.collapsesSingle,
        .disabled = strip.disabled,
    }, strip.style);
    if (jsKitNodeIsPlaced(round.tree, node))
        prKitPlace(kit, made, jsKitNodePlace(round.tree, node));
    tabStripQueueMove(round, node, strip, made);

    // The tabs select by a click and by the arrow keys as they are declared, so the strip declares them itself, and
    // tells the script of the selection that they leave in the same round.
    jsKitDeclareChildren(round, node, made);
    uint64_t selected = prKitTabsSelected(strip.tabs);
    if (selected != strip.selected) {
        strip.selected = selected;
        JsKitDetail detail{.values = {double(selected)}, .valueCount = 1};
        jsKitEventPush(round, node, TabStripEvent_Select, &detail);
    }
    return kPrLayoutNodeNone;
}

PrLayoutNode tabDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    if (!jsKitNodeIsUnder(tree, node, "tab-strip"))
        return kPrLayoutNodeNone;
    const TabData& tab = jsKitData<TabData>(round, node);
    if (tab.id == 0)
        return kPrLayoutNodeNone;
    // The strip holds the first tab of an id, and the kit creates a tab once.
    for (JsKitNodeIndex before = jsKitNodeFirstChild(tree, jsKitNodeParent(tree, node)); before != node;
            before = jsKitNodeNextSibling(tree, before)) {
        if (isTab(tree, before) && tabId(tree, before) == tab.id)
            return kPrLayoutNodeNone;
    }
    return prKitTab(round.kit, parent, {.id = tab.id, .disabled = tab.disabled});
}

PrLayoutNode tabCloseDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    if (!jsKitNodeIsUnder(tree, node, "tab"))
        return kPrLayoutNodeNone;
    PrLayoutNode made = prKitTabClose(round.kit, parent);
    if (prKitActivated(round.kit, made)) {
        JsKitNodeIndex tab = jsKitNodeParent(tree, node);
        JsKitDetail detail{.values = {double(tabId(tree, tab))}, .valueCount = 1};
        jsKitEventPush(round, tab, TabEvent_Remove, &detail);
    }
    return made;
}

// ---------------------------------------------------------------------------------------------------------------------
// Progress

constexpr JsKitKind kKinds[] = {
    {
        .name = "checkbox",
        .dataSize = sizeof(CheckData),
        .events = kCheckEvents,
        .eventCount = 1,
        .set = checkSet,
        .declare = checkboxDeclare,
    },
    {
        .name = "toggle",
        .dataSize = sizeof(CheckData),
        .events = kCheckEvents,
        .eventCount = 1,
        .set = checkSet,
        .declare = toggleDeclare,
    },
    {
        .name = "tab-strip",
        .dataSize = sizeof(TabStripData),
        .events = kTabStripEvents,
        .eventCount = 3,
        .flags = JsKitKindFlag_PlacesItself,
        .create = tabStripCreate,
        .destroy = tabStripDestroy,
        .set = tabStripSet,
        .declare = tabStripDeclare,
    },
    {
        .name = "tab",
        .dataSize = sizeof(TabData),
        .events = kTabEvents,
        .eventCount = 1,
        .set = tabSet,
        .declare = tabDeclare,
    },
    {.name = "tab-close", .declare = tabCloseDeclare},
};

} // namespace

Span<const JsKitKind> jsKitChoiceKinds() {
    return kKinds;
}

} // namespace pr_impl
