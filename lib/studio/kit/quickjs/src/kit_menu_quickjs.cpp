// The kinds of Studio Kit's menus, popovers, tooltips and dialogs (pr/kit_menu.h). An attribute sets the field of its
// name in the content or the style of the kind's widget, which the header describes, unless it is described here. One
// that is given here with an enumeration takes the names of its members as keywords, in lower case with a hyphen
// between words, and a reset leaves the first of them.
//
//   popover         A raised panel. `offset`, its x and y in px from its trigger's bottom left corner, `min-width` in
//                   px, `open` and `triggered`. Holds nodes of any kind. Queues `toggle`.
//   menu            A column of items, which goes anywhere, and holds the keyboard in a popover. `min-width` in px, and
//                   `leaves-focus`. Holds `menu-label`, `menu-item` and `menu-separator` nodes, or any others.
//   menu-label      A caption above a group of items, which shows its node's text.
//   menu-item       Shows its node's text. `shortcut`, a text, `icon`, an icon's name (prKitIconName), `checked`,
//                   `disabled`, and `variant` (PrKitMenuItemVariant). Queues `click`.
//   menu-separator  A rule between groups of items.
//   select          A field that shows its node's text, which is the label of the chosen option. `icon`, `disabled`,
//                   and `size` (PrKitControlSize). Holds a `select-menu`. Queues `step`.
//   select-menu     Under a `select` alone. The menu of the select's options. `open`. Holds a `menu-item` for each
//                   option, the chosen one `checked`. Queues `toggle`.
//   menu-bar        `plain`. Holds `menu-bar-item` nodes.
//   menu-bar-item   Under a `menu-bar` alone. Shows its node's text. `disabled`. Holds its menu, as a `popover` with a
//                   `menu` under it.
//   context-menu    The menu that a secondary click on the node it is under, its host, opens at the pointer.
//                   `min-width` in px, `leaves-focus`, `open`, and `at`, the x and y in px of the host's viewport
//                   where `open` opens it. Holds what a `menu` holds. Queues `toggle`.
//   tooltip         Shows its node's text below the node it is under, once the pointer has rested there. `shortcut`, a
//                   text.
//   drag-ghost      Shows its node's text beside the pointer while a payload is dragged, wherever the node is in the
//                   tree. `icon`.
//   modal           A column over a scrim. `placement` (PrKitModalPlacement), `inset` in px, `open` and `triggered`.
//                   Holds nodes of any kind. Queues `toggle`.
//   modal-autofocus Makes the node it is under the one that takes focus as the modal, the dialog or the palette that
//                   holds that node opens. It does nothing under a node that takes no focus.
//   dialog          A modal card whose title is its node's text. `placement`, `inset`, `width` in px, `open` and
//                   `triggered`. Holds the nodes of the card's body, of any kind. Queues `toggle`.
//   dialog-footer   A row that puts the nodes under it at its end, which goes last in a dialog for its buttons.
//   command-palette A modal list under a search row. `prompt`, a text, `query`, the text that the search row edits,
//                   `searchable`, `open` and `triggered`. Holds `command-item` nodes, and a `command-palette-footer`
//                   after them. Queues `toggle` and `change`.
//   command-item    Under a `command-palette` alone. Shows its node's text. `category` and `shortcut`, texts, `icon`,
//                   and `disabled`. Queues `click`.
//   command-palette-footer
//                   Under a `command-palette` alone. The row of key hints at the end of the palette's card.
//
// A node that is not under the kind it goes under is not declared, and neither are the nodes under it.
//
// Overlays. A floating widget is a node under the node that it floats from: its trigger, its host or its anchor. A
// button that opens a popover is a `button` with a `popover` under it. An overlay takes no room in the node it is
// under, and the nodes under the overlay are declared only while it shows.
//
// Open state. A `popover`, a `select-menu`, a `modal`, a `dialog`, a `command-palette` and a `context-menu` take
// `open`, a flag that the script sets to open the overlay and resets to close it. Each queues `toggle` when it opens or
// closes other than by `open`, with 1 as its `value` if it is now open and 0 if it is now closed: on Escape, on a press
// outside it, on the activation of one of its items, or as its trigger opens it. The overlay stays as the event says
// until the script sets `open` to the other state. A tooltip and a drag's ghost show by the kit's rule alone.
//
// The kit keeps one open state on a node for whichever popover, select's menu, modal, dialog or palette is under it. A
// node therefore opens the first of these under it, and the others under it are not declared. A context menu's state is
// apart from that, and a node has one context menu, the first under it. An overlay that leaves the tree while it is
// open is closed, and says so when it returns.
//
// Triggers. A `select` and a `menu-bar-item` open and close what is under them as they are activated. A `context-menu`
// opens on a secondary click on its host. For any other node, `triggered` on a `popover`, a `modal`, a `dialog` or a
// `command-palette` makes an activation of the node that the overlay is under open the overlay, and close a popover
// that is open, with no script. An overlay without it opens when the script sets `open`, as on its trigger's `click`.
//
// The other events:
//
//   click    A `menu-item` or a `command-item` was activated, which closes the popover, the select's menu, the context
//            menu or the palette that it is in.
//   step     Up or Down was pressed while a `select` had focus and its menu was closed. Its `value` is -1 or 1, and the
//            script chooses the option before or after the chosen one.
//   toggle   Of a `context-menu` that a click opened, the `values` after the 1 are the x and y of the click, in px of
//            the host's viewport. A context menu that an item's activation closed says so in the next round.
//   change   The user edited the query of a `command-palette`. Its `text` is the new query, which the search row keeps
//            until the script sets `query`. The script removes the items that the query does not match.
//
// Placement. `min-width` and `width` above are the attributes of those names that every kind takes, which a value in px
// sets in the kit's own style of these kinds. A value of another form, and every other attribute of a node's place,
// places the widget that the nodes under the node are declared in: a popover's panel, a menu, a modal's column, a
// dialog's body or a palette's list.

#include "studio/kit/quickjs/src/kit_menu_quickjs.h"

#include "pr/kit_input.h"
#include "pr/kit_menu.h"
#include "pr/mem.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kItemVariants[] = {
    {"default", PrKitMenuItemVariant_Default},
    {"danger", PrKitMenuItemVariant_Danger},
};

constexpr JsKitKeyword kModalPlacements[] = {{"center", PrKitModalPlacement_Center}, {"top", PrKitModalPlacement_Top}};

typedef PrLayoutNode (*DeclareFn)(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);

PrLayoutNode popoverDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode selectDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode selectMenuDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode menuBarItemDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode contextMenuDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode modalDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode dialogDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode commandPaletteDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);
PrLayoutNode commandItemDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent);

// Returns true if the node's parent in the tree is of the kind that `declare` declares. A part that the kit creates
// under its container alone is declared only if this holds, since a script may put a node under any other.
bool isUnder(const JsKitNodeTree* tree, JsKitNodeIndex node, DeclareFn declare) {
    return jsKitNodeKind(tree, jsKitNodeParent(tree, node))->declare == declare;
}

// Returns true if `kind` is an overlay whose open state the kit keeps on its trigger (prKitIsOpen).
bool opensByState(const JsKitKind* kind) {
    DeclareFn declare = kind->declare;
    return declare == popoverDeclare || declare == selectMenuDeclare || declare == modalDeclare ||
        declare == dialogDeclare || declare == commandPaletteDeclare;
}

bool isContextMenu(const JsKitKind* kind) {
    return kind->declare == contextMenuDeclare;
}

// Returns true if no node before `node` under its parent is of a kind that `shares` accepts: the kinds that share one
// state of the kit's on the parent's widget, which the first of them alone is declared with.
bool isFirstOf(const JsKitNodeTree* tree, JsKitNodeIndex node, bool (*shares)(const JsKitKind* kind)) {
    for (JsKitNodeIndex before = jsKitNodeFirstChild(tree, jsKitNodeParent(tree, node)); before != node;
            before = jsKitNodeNextSibling(tree, before)) {
        if (shares(jsKitNodeKind(tree, before)))
            return false;
    }
    return true;
}

// Reads a point, as its x and its y separated by spaces, or with `reset` writes the origin.
bool setPoint(PrStringView value, bool reset, PrVec2* out) {
    if (reset) {
        *out = PrVec2{};
        return true;
    }
    double numbers[2] = {};
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
        if (count == 2 || !jsKitNumberFrom(PrStringView{value.data + at, end - at}, &numbers[count]))
            return false;
        ++count;
        at = end;
    }
    if (count != 2)
        return false;
    *out = PrVec2{float(numbers[0]), float(numbers[1])};
    return true;
}

// Takes a size in px out of a node's place, for a style of the kit's that holds the same measure in px. Returns 0, and
// leaves the size in the place, if it is not in px.
float takePx(PrKitSize& size) {
    if (size.kind != PrKitSizeKind_Px)
        return 0;
    float px = size.value;
    size = PrKitSize{};
    return px;
}

// Places a widget as `place` says, if it says anything.
void placeIfSet(PrKit* kit, PrLayoutNode made, const PrKitPlace& place) {
    PrKitPlace none{};
    if (!prMemEquals(&place, &none, sizeof place))
        prKitPlace(kit, made, place);
}

// Declares the nodes under an overlay's node in `content`, the widget that the overlay shows them in, which is first
// placed by `place`.
void declareContent(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode content, const PrKitPlace& place) {
    placeIfSet(round.kit, content, place);
    jsKitDeclareChildren(round, node, content);
}

// ---------------------------------------------------------------------------------------------------------------------
// Open state

// Whether an overlay is open. The kit keeps the state on the overlay's trigger and changes it as the user dismisses the
// overlay. This is the script's side of it.
struct OpenData {
    bool open;                      // as the script last set `open`, or as its last `toggle` said
    bool changed;                   // the script changed `open` since the overlay was last declared
    bool triggered;                 // an activation of the trigger opens the overlay
};

// The event of every overlay, by its index.
enum { OverlayEvent_Toggle };
constexpr PrStringView kOverlayEvents[] = {"toggle"};

// Sets `open` from `value`.
bool openSet(OpenData& state, PrStringView value, bool reset) {
    bool open = false;
    if (!jsKitSetFlag(value, reset, &open))
        return false;
    state.changed = state.changed || open != state.open;
    state.open = open;
    return true;
}

// Opens or closes the overlay of `trigger` as the script set `open` since the overlay was last declared.
void openApply(PrKit* kit, OpenData& state, PrLayoutNode trigger) {
    if (!state.changed)
        return;
    state.changed = false;
    if (prKitIsOpen(kit, trigger) == state.open)
        return;
    prKitSetOpen(kit, trigger, state.open);
    // A trigger that shows whether it is open, as a select does, was declared before this, and shows it in the frame
    // that this asks for.
    prUiRequestFrame(kit->ui);
}

// Opens a triggered overlay if its trigger was activated in the round.
void openOnActivation(PrKit* kit, const OpenData& state, PrLayoutNode trigger) {
    if (state.triggered && prKitActivated(kit, trigger))
        prKitSetOpen(kit, trigger, true);
}

// Queues `toggle` if the overlay of `trigger` is not as the script last knew it, which is how the script hears that the
// round opened or closed the overlay. Call once the nodes under the overlay are declared, since an item's activation
// closes the overlay that it is in.
void openSettle(const JsKitRound& round, JsKitNodeIndex node, OpenData& state, PrLayoutNode trigger) {
    bool open = prKitIsOpen(round.kit, trigger);
    if (open == state.open)
        return;
    state.open = open;
    JsKitDetail detail{.values = {open ? 1.0 : 0.0}, .valueCount = 1};
    jsKitEventPush(round, node, OverlayEvent_Toggle, &detail);
}

// ---------------------------------------------------------------------------------------------------------------------
// Popovers

struct PopoverData {
    PrVec2 offset;
    OpenData open;
};

bool popoverSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PopoverData& popover = *static_cast<PopoverData*>(data);
    if (name == "offset")
        return setPoint(value, reset, &popover.offset);
    if (name == "open")
        return openSet(popover.open, value, reset);
    if (name == "triggered")
        return jsKitSetFlag(value, reset, &popover.open.triggered);
    return false;
}

PrLayoutNode popoverDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    if (!isFirstOf(tree, node, opensByState))
        return kPrLayoutNodeNone;
    PopoverData& popover = jsKitData<PopoverData>(round, node);
    PrKit* kit = round.kit;
    openApply(kit, popover.open, parent);
    // A select and a menu bar's item open and close themselves as they are activated.
    bool opensItself = isUnder(tree, node, selectDeclare) || isUnder(tree, node, menuBarItemDeclare);
    if (popover.open.triggered && !opensItself && prKitActivated(kit, parent))
        prKitToggleOpen(kit, parent);
    PrKitPlace place = jsKitNodePlace(tree, node);
    PrKitPopoverStyle style{.offset = popover.offset, .minWidth = takePx(place.minWidth)};
    PrLayoutNode panel = prKitPopover(kit, parent, {.key = jsKitNodeKey(round, node)}, style);
    if (panel != kPrLayoutNodeNone)
        declareContent(round, node, panel, place);
    openSettle(round, node, popover.open, parent);
    return kPrLayoutNodeNone;
}

// ---------------------------------------------------------------------------------------------------------------------
// Menus

struct MenuItemData {
    PrRawString shortcut;
    PrKitIconKind icon;
    PrKitMenuItemStyle style;
    bool checked;
    bool disabled;
};

// The events of a menu item and of a command item, by their index.
enum { ItemEvent_Click };
constexpr PrStringView kItemEvents[] = {"click"};

bool menuSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "leaves-focus")
        return jsKitSetFlag(value, reset, static_cast<bool*>(data));
    return false;
}

PrLayoutNode menuDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitPlace place = jsKitNodePlace(round.tree, node);
    PrKitMenuStyle style{.minWidth = takePx(place.minWidth), .leavesFocus = jsKitData<bool>(round, node)};
    PrLayoutNode made = prKitMenu(round.kit, parent, {.key = jsKitNodeKey(round, node)}, style);
    placeIfSet(round.kit, made, place);
    return made;
}

PrLayoutNode menuLabelDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitMenuLabel label{.key = jsKitNodeKey(round, node), .text = jsKitNodeText(round.tree, node)};
    return prKitMenuLabel(round.kit, parent, label);
}

void menuItemDestroy(void* data) {
    prRawStringReset(&static_cast<MenuItemData*>(data)->shortcut);
}

bool menuItemSet(void* data, PrStringView name, PrStringView value, bool reset) {
    MenuItemData& item = *static_cast<MenuItemData*>(data);
    if (name == "shortcut")
        return jsKitSetString(value, reset, &item.shortcut);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &item.icon);
    if (name == "checked")
        return jsKitSetFlag(value, reset, &item.checked);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &item.disabled);
    if (name == "variant")
        return jsKitSetKeyword(kItemVariants, value, reset, &item.style.variant);
    return false;
}

PrLayoutNode menuItemDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const MenuItemData& item = jsKitData<MenuItemData>(round, node);
    PrLayoutNode made = prKitMenuItem(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .shortcut = prRawStringView(&item.shortcut),
        .icon = item.icon,
        .checked = item.checked,
        .disabled = item.disabled,
    }, item.style);
    if (prKitActivated(round.kit, made))
        jsKitEventPush(round, node, ItemEvent_Click, nullptr);
    return made;
}

PrLayoutNode menuSeparatorDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return prKitMenuSeparator(round.kit, parent, {.key = jsKitNodeKey(round, node)});
}

// ---------------------------------------------------------------------------------------------------------------------
// Selects

struct SelectData {
    PrKitChoiceStyle style;
    PrKitIconKind icon;
    bool disabled;
};

// The events of a select, by their index.
enum { SelectEvent_Step };
constexpr PrStringView kSelectEvents[] = {"step"};

bool selectSet(void* data, PrStringView name, PrStringView value, bool reset) {
    SelectData& select = *static_cast<SelectData*>(data);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &select.icon);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &select.disabled);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &select.style.size);
    return false;
}

PrLayoutNode selectDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const SelectData& select = jsKitData<SelectData>(round, node);
    PrLayoutNode made = prKitSelect(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .label = jsKitNodeText(round.tree, node),
        .icon = select.icon,
        .disabled = select.disabled,
    }, select.style);
    int32_t step = 0;
    if (prKitStepped(round.kit, made, &step)) {
        JsKitDetail detail{.values = {double(step)}, .valueCount = 1};
        jsKitEventPush(round, node, SelectEvent_Step, &detail);
    }
    return made;
}

bool selectMenuSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "open")
        return openSet(*static_cast<OpenData*>(data), value, reset);
    return false;
}

PrLayoutNode selectMenuDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    if (!isUnder(tree, node, selectDeclare) || !isFirstOf(tree, node, opensByState))
        return kPrLayoutNodeNone;
    OpenData& open = jsKitData<OpenData>(round, node);
    openApply(round.kit, open, parent);
    PrLayoutNode menu = prKitSelectMenu(round.kit, parent);
    if (menu != kPrLayoutNodeNone)
        declareContent(round, node, menu, jsKitNodePlace(tree, node));
    openSettle(round, node, open, parent);
    return kPrLayoutNodeNone;
}

// ---------------------------------------------------------------------------------------------------------------------
// Menu bars

bool menuBarSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "plain")
        return jsKitSetFlag(value, reset, static_cast<bool*>(data));
    return false;
}

PrLayoutNode menuBarDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitMenuBar bar{.key = jsKitNodeKey(round, node), .plain = jsKitData<bool>(round, node)};
    return prKitMenuBar(round.kit, parent, bar);
}

bool menuBarItemSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "disabled")
        return jsKitSetFlag(value, reset, static_cast<bool*>(data));
    return false;
}

PrLayoutNode menuBarItemDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isUnder(round.tree, node, menuBarDeclare))
        return kPrLayoutNodeNone;
    return prKitMenuBarItem(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .disabled = jsKitData<bool>(round, node),
    });
}

// ---------------------------------------------------------------------------------------------------------------------
// Context menus

struct ContextMenuData {
    PrVec2 at;                      // where `open` opens the menu, in px of the host's viewport
    OpenData open;
    bool leavesFocus;
};

bool contextMenuSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ContextMenuData& context = *static_cast<ContextMenuData*>(data);
    if (name == "at")
        return setPoint(value, reset, &context.at);
    if (name == "open")
        return openSet(context.open, value, reset);
    if (name == "leaves-focus")
        return jsKitSetFlag(value, reset, &context.leavesFocus);
    return false;
}

PrLayoutNode contextMenuDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isFirstOf(round.tree, node, isContextMenu))
        return kPrLayoutNodeNone;
    ContextMenuData& context = jsKitData<ContextMenuData>(round, node);
    PrKit* kit = round.kit;
    if (context.open.changed) {
        context.open.changed = false;
        if (context.open.open)
            prKitOpenContextMenu(kit, parent, context.at);
        else
            prKitCloseContextMenu(kit, parent);
    }
    PrKitPlace place = jsKitNodePlace(round.tree, node);
    PrKitMenuStyle style{.minWidth = takePx(place.minWidth), .leavesFocus = context.leavesFocus};
    PrLayoutNode menu = prKitContextMenu(kit, parent, {.key = jsKitNodeKey(round, node)}, style);

    // The kit tells whether a context menu is open only by what this call returns. An item's activation closes the menu
    // after the call, so the script hears of that close in the next round.
    bool open = menu != kPrLayoutNodeNone;
    if (open != context.open.open) {
        context.open.open = open;
        JsKitDetail detail{.values = {open ? 1.0 : 0.0}, .valueCount = 1};
        PrVec2 pointer{};
        if (open && prUiPointer(kit->ui, nullptr, &pointer)) {
            detail.values[1] = double(pointer.x);
            detail.values[2] = double(pointer.y);
            detail.valueCount = 3;
        }
        jsKitEventPush(round, node, OverlayEvent_Toggle, &detail);
    }
    if (open)
        placeIfSet(kit, menu, place);
    return menu;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tooltips and drag ghosts

void tooltipDestroy(void* data) {
    prRawStringReset(static_cast<PrRawString*>(data));
}

bool tooltipSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "shortcut")
        return jsKitSetString(value, reset, static_cast<PrRawString*>(data));
    return false;
}

PrLayoutNode tooltipDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return prKitTooltip(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .shortcut = prRawStringView(&jsKitData<PrRawString>(round, node)),
    });
}

bool dragGhostSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "icon")
        return jsKitSetIcon(value, reset, static_cast<PrKitIconKind*>(data));
    return false;
}

PrLayoutNode dragGhostDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode) {
    return prKitDragGhost(round.kit, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .icon = jsKitData<PrKitIconKind>(round, node),
    });
}

// ---------------------------------------------------------------------------------------------------------------------
// Modals and dialogs

// A modal or a dialog. A dialog takes its width from its node's place.
struct ModalData {
    PrKitModalStyle style;
    OpenData open;
};

bool modalSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ModalData& modal = *static_cast<ModalData*>(data);
    if (name == "placement")
        return jsKitSetKeyword(kModalPlacements, value, reset, &modal.style.placement);
    if (name == "inset")
        return jsKitSetLength(value, reset, &modal.style.inset);
    if (name == "open")
        return openSet(modal.open, value, reset);
    if (name == "triggered")
        return jsKitSetFlag(value, reset, &modal.open.triggered);
    return false;
}

PrLayoutNode modalDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isFirstOf(round.tree, node, opensByState))
        return kPrLayoutNodeNone;
    ModalData& modal = jsKitData<ModalData>(round, node);
    PrKit* kit = round.kit;
    openApply(kit, modal.open, parent);
    openOnActivation(kit, modal.open, parent);
    PrLayoutNode content = prKitModal(kit, parent, {.key = jsKitNodeKey(round, node)}, modal.style);
    if (content != kPrLayoutNodeNone)
        declareContent(round, node, content, jsKitNodePlace(round.tree, node));
    openSettle(round, node, modal.open, parent);
    return kPrLayoutNodeNone;
}

PrLayoutNode modalAutofocusDeclare(const JsKitRound& round, JsKitNodeIndex, PrLayoutNode parent) {
    // The kit gives focus to the node that it is told of, and a node that takes none would leave the modal without
    // focus in every round.
    PrUi* ui = round.kit->ui;
    if ((prUiFlags(ui, parent) & PrUiFlag_Focusable) != 0 && !prUiIsDisabled(ui, parent))
        prKitModalAutofocus(round.kit, parent);
    return kPrLayoutNodeNone;
}

PrLayoutNode dialogDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isFirstOf(round.tree, node, opensByState))
        return kPrLayoutNodeNone;
    ModalData& dialog = jsKitData<ModalData>(round, node);
    PrKit* kit = round.kit;
    openApply(kit, dialog.open, parent);
    openOnActivation(kit, dialog.open, parent);
    PrKitPlace place = jsKitNodePlace(round.tree, node);
    PrKitDialog content{.key = jsKitNodeKey(round, node), .title = jsKitNodeText(round.tree, node)};
    PrKitDialogStyle style{.modal = dialog.style, .width = takePx(place.width)};
    PrLayoutNode body = prKitDialog(kit, parent, content, style);
    if (body != kPrLayoutNodeNone)
        declareContent(round, node, body, place);
    openSettle(round, node, dialog.open, parent);
    return kPrLayoutNodeNone;
}

PrLayoutNode dialogFooterDeclare(const JsKitRound& round, JsKitNodeIndex, PrLayoutNode parent) {
    return prKitDialogFooter(round.kit, parent);
}

// ---------------------------------------------------------------------------------------------------------------------
// Command palettes

// A command palette. The kit edits the query in a buffer of its own, and the node keeps what it reports.
struct PaletteData {
    PrRawString prompt;
    PrRawString query;
    OpenData open;
    bool searchable;
};

struct CommandItemData {
    PrRawString category;
    PrRawString shortcut;
    PrKitIconKind icon;
    bool disabled;
};

// The events of a command palette, by their index. Its `toggle` is every overlay's.
enum { PaletteEvent_Change = OverlayEvent_Toggle + 1 };
constexpr PrStringView kPaletteEvents[] = {"toggle", "change"};

void commandPaletteDestroy(void* data) {
    PaletteData& palette = *static_cast<PaletteData*>(data);
    prRawStringReset(&palette.prompt);
    prRawStringReset(&palette.query);
}

bool commandPaletteSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PaletteData& palette = *static_cast<PaletteData*>(data);
    if (name == "prompt")
        return jsKitSetString(value, reset, &palette.prompt);
    if (name == "query")
        return jsKitSetString(value, reset, &palette.query);
    if (name == "searchable")
        return jsKitSetFlag(value, reset, &palette.searchable);
    if (name == "open")
        return openSet(palette.open, value, reset);
    if (name == "triggered")
        return jsKitSetFlag(value, reset, &palette.open.triggered);
    return false;
}

PrLayoutNode commandPaletteDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const JsKitNodeTree* tree = round.tree;
    if (!isFirstOf(tree, node, opensByState))
        return kPrLayoutNodeNone;
    PaletteData& palette = jsKitData<PaletteData>(round, node);
    PrKit* kit = round.kit;
    openApply(kit, palette.open, parent);
    openOnActivation(kit, palette.open, parent);
    uint32_t count = 0;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child))
        count += jsKitNodeKind(tree, child)->declare == commandItemDeclare ? 1 : 0;
    PrLayoutNode list = prKitCommandPalette(kit, parent, {
        .key = jsKitNodeKey(round, node),
        .prompt = prRawStringView(&palette.prompt),
        .count = count,
        .query = prRawStringView(&palette.query),
        .searchable = palette.searchable,
    });
    if (list != kPrLayoutNodeNone) {
        PrStringView query = {};
        if (prKitTextChanged(kit, list, &query)) {
            JsKitDetail detail{.hasText = true, .text = query};
            jsKitEventPush(round, node, PaletteEvent_Change, &detail);
            prRawStringReplace(&palette.query, query);
        }
        declareContent(round, node, list, jsKitNodePlace(tree, node));
    }
    openSettle(round, node, palette.open, parent);
    return kPrLayoutNodeNone;
}

void commandItemDestroy(void* data) {
    CommandItemData& item = *static_cast<CommandItemData*>(data);
    prRawStringReset(&item.category);
    prRawStringReset(&item.shortcut);
}

bool commandItemSet(void* data, PrStringView name, PrStringView value, bool reset) {
    CommandItemData& item = *static_cast<CommandItemData*>(data);
    if (name == "category")
        return jsKitSetString(value, reset, &item.category);
    if (name == "shortcut")
        return jsKitSetString(value, reset, &item.shortcut);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &item.icon);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &item.disabled);
    return false;
}

PrLayoutNode commandItemDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isUnder(round.tree, node, commandPaletteDeclare))
        return kPrLayoutNodeNone;
    const CommandItemData& item = jsKitData<CommandItemData>(round, node);
    PrLayoutNode made = prKitCommandItem(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .category = prRawStringView(&item.category),
        .shortcut = prRawStringView(&item.shortcut),
        .icon = item.icon,
        .disabled = item.disabled,
    });
    if (prKitActivated(round.kit, made))
        jsKitEventPush(round, node, ItemEvent_Click, nullptr);
    return made;
}

PrLayoutNode commandPaletteFooterDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    if (!isUnder(round.tree, node, commandPaletteDeclare))
        return kPrLayoutNodeNone;
    return prKitCommandPaletteFooter(round.kit, parent);
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "popover",
        .dataSize = sizeof(PopoverData),
        .events = kOverlayEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = popoverSet,
        .declare = popoverDeclare,
    },
    {
        .name = "menu",
        .dataSize = sizeof(bool),
        .flags = JsKitKindFlag_PlacesItself,
        .set = menuSet,
        .declare = menuDeclare,
    },
    {.name = "menu-label", .declare = menuLabelDeclare},
    {
        .name = "menu-item",
        .dataSize = sizeof(MenuItemData),
        .events = kItemEvents,
        .eventCount = 1,
        .destroy = menuItemDestroy,
        .set = menuItemSet,
        .declare = menuItemDeclare,
    },
    {.name = "menu-separator", .declare = menuSeparatorDeclare},
    {
        .name = "select",
        .dataSize = sizeof(SelectData),
        .events = kSelectEvents,
        .eventCount = 1,
        .set = selectSet,
        .declare = selectDeclare,
    },
    {
        .name = "select-menu",
        .dataSize = sizeof(OpenData),
        .events = kOverlayEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = selectMenuSet,
        .declare = selectMenuDeclare,
    },
    {.name = "menu-bar", .dataSize = sizeof(bool), .set = menuBarSet, .declare = menuBarDeclare},
    {.name = "menu-bar-item", .dataSize = sizeof(bool), .set = menuBarItemSet, .declare = menuBarItemDeclare},
    {
        .name = "context-menu",
        .dataSize = sizeof(ContextMenuData),
        .events = kOverlayEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = contextMenuSet,
        .declare = contextMenuDeclare,
    },
    {
        .name = "tooltip",
        .dataSize = sizeof(PrRawString),
        .destroy = tooltipDestroy,
        .set = tooltipSet,
        .declare = tooltipDeclare,
    },
    {.name = "drag-ghost", .dataSize = sizeof(PrKitIconKind), .set = dragGhostSet, .declare = dragGhostDeclare},
    {
        .name = "modal",
        .dataSize = sizeof(ModalData),
        .events = kOverlayEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = modalSet,
        .declare = modalDeclare,
    },
    {.name = "modal-autofocus", .declare = modalAutofocusDeclare},
    {
        .name = "dialog",
        .dataSize = sizeof(ModalData),
        .events = kOverlayEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = modalSet,
        .declare = dialogDeclare,
    },
    {.name = "dialog-footer", .declare = dialogFooterDeclare},
    {
        .name = "command-palette",
        .dataSize = sizeof(PaletteData),
        .events = kPaletteEvents,
        .eventCount = 2,
        .flags = JsKitKindFlag_PlacesItself,
        .destroy = commandPaletteDestroy,
        .set = commandPaletteSet,
        .declare = commandPaletteDeclare,
    },
    {
        .name = "command-item",
        .dataSize = sizeof(CommandItemData),
        .events = kItemEvents,
        .eventCount = 1,
        .destroy = commandItemDestroy,
        .set = commandItemSet,
        .declare = commandItemDeclare,
    },
    {.name = "command-palette-footer", .declare = commandPaletteFooterDeclare},
};

} // namespace

Span<const JsKitKind> jsKitMenuKinds() {
    return kKinds;
}

} // namespace pr_impl
