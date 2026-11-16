// The kinds of Studio Kit's menus, popovers, tooltips and dialogs in its QuickJS binding: each declares what the same
// calls to the kit declare.

#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_choice.h"
#include "pr/kit_input.h"
#include "pr/kit_menu.h"
#include "pr/kit_text.h"
#include "pr/string.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// A tree that a script makes, and a kit that a test declares the same widgets in with the kit's own calls. Both take
// the same frames and the same input.
struct Pair {
    JsKitScripted scripted;
    KitFixture direct;

    void frame(uint32_t count = 1, double delta = 1.0 / 60) {
        for (uint32_t i = 0; i < count; ++i) {
            scripted.fixture.frame(delta);
            direct.frame(delta);
        }
    }

    // Runs a frame that ends with the round that routes a click's release. A context routes the press in a frame's
    // first round and the release in its second, and a third round would declare what the click changed.
    void clickRounds() {
        scripted.fixture.frame(1.0 / 60, 2);
        direct.frame(1.0 / 60, 2);
    }

    void push(PrAppEvent event) {
        scripted.fixture.push(event);
        direct.push(event);
    }
    void move(PrVec2 at) {
        scripted.fixture.move(at);
        direct.move(at);
    }
    void click(PrVec2 at) {
        scripted.fixture.click(at);
        direct.click(at);
    }
    void secondaryClick(PrVec2 at) {
        push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = at, .button = 1}});
        push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = at, .button = 1}});
    }
    void key(PrAppKey key) {
        scripted.fixture.key(key);
        direct.key(key);
    }
    void type(PrStringView text) {
        scripted.fixture.type(text);
        direct.type(text);
    }

    // Returns true if both render the same pixels, and the script's tree is laid out as a layout from scratch lays it
    // out. An overlay is in the tree that a fixture renders, since a fixture has no host to show it in a window of its
    // own.
    bool alike() { return kitVerified(scripted.fixture.tree) && jsKitRendersAlike(scripted.fixture, direct); }

    // Returns true if focus is on widgets with the same bounds in both, or on none in both.
    bool focusAlike() {
        PrLayoutNode focuses[2] = {prUiFocus(scripted.fixture.ui), prUiFocus(direct.ui)};
        if (focuses[0] == kPrLayoutNodeNone || focuses[1] == kPrLayoutNodeNone)
            return focuses[0] == focuses[1];
        PrRect a = scripted.fixture.bounds(focuses[0]);
        PrRect b = direct.bounds(focuses[1]);
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }
};

// Returns true if the fixture renders a pixel in `bounds` that differs from `before`, an earlier render of it. This is
// how a test knows that an overlay is among the pixels that it compares.
bool rendersIn(KitFixture& fixture, const Array<uint8_t>& before, PrRect bounds) {
    Array<uint8_t> now;
    if (!fixture.render(&now) || now.count() != before.count())
        return false;
    for (uint32_t y = uint32_t(bounds.top); y < uint32_t(bounds.bottom) && y < KitFixture::kRenderHeight; ++y) {
        for (uint32_t x = uint32_t(bounds.left); x < uint32_t(bounds.right) && x < KitFixture::kRenderWidth; ++x) {
            uint32_t at = (y * KitFixture::kRenderWidth + x) * 4;
            if (!prMemEquals(now.data() + at, before.data() + at, 4))
                return true;
        }
    }
    return false;
}

// A script's record of the events that it hears, and the functions that a test's script listens with. `hear` listens
// for a node's events of a type, with a function to call for each, and returns the node. `heard` counts the events of a
// type that a node queued, and `last` returns the latest of them.
constexpr const char* kHear = R"JS(
    var log = [];
    var listeners = {};
    kitTree.onevent = function (node, type, detail) {
        log.push({node: node, type: type, value: detail.value, values: detail.values, text: detail.text});
        const listener = listeners[node + ' ' + type];
        if (listener)
            listener(detail);
    };
    function hear(node, type, listener) {
        if (!kitTree.listen(node, type, true))
            throw new Error('node ' + node + ' queues no ' + type);
        listeners[node + ' ' + type] = listener;
        return node;
    }
    function heard(node, type) {
        return log.filter(function (event) { return event.node === node && event.type === type; }).length;
    }
    function last(node, type) {
        return log.filter(function (event) { return event.node === node && event.type === type; }).pop();
    }
)JS";

// The kit's tabs of a strip whose tab a test drags, with the ids 1 and 2.
struct TwoTabs {
    PrKitTabs* tabs = prKitTabsCreate();
    TwoTabs() {
        prKitTabsInsert(tabs, 1, 0);
        prKitTabsInsert(tabs, 2, 1);
    }
    TwoTabs(const TwoTabs&) = delete;
    TwoTabs& operator=(const TwoTabs&) = delete;
    ~TwoTabs() { prKitTabsDestroy(tabs); }
};

// Returns true if `text` holds `query`, whatever the case of their ASCII letters.
bool holds(PrStringView text, PrStringView query) {
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + 32) : c; };
    for (size_t start = 0; start + query.size <= text.size; ++start) {
        size_t matched = 0;
        while (matched < query.size && lower(text.data[start + matched]) == lower(query.data[matched]))
            ++matched;
        if (matched == query.size)
            return true;
    }
    return false;
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a triggered popover opens on its trigger's click with its menu as the kit shows it, and an item's click closes it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '16'});
        var trigger = make('button', row, {}, 'File');
        var popover = make('popover', trigger, {triggered: '', 'min-width': '240', offset: '4 2'});
        // The script keeps `open` as each toggle says, as a component that binds it does, which changes nothing.
        hear(popover, 'toggle', function (detail) { kitTree.set(popover, 'open', detail.value ? '' : null); });
        var menu = make('menu', popover, {'min-width': '200', margin: '4'});
        make('menu-label', menu, {}, 'SCENE');
        var create = hear(make('menu-item', menu, {shortcut: 'Cmd N', icon: 'plus'}, 'New Scene'), 'click');
        make('menu-separator', menu, {});
        var grid = hear(make('menu-item', menu, {checked: ''}, 'Grid'), 'click');
        var remove = hear(make('menu-item', menu, {disabled: '', variant: 'danger'}, 'Delete'), 'click');
        var other = hear(make('button', row, {}, 'Other'), 'click');
    )JS");
    PrLayoutNode trigger = kPrLayoutNodeNone;
    PrLayoutNode other = kPrLayoutNodeNone;
    PrLayoutNode popover = kPrLayoutNodeNone;
    PrLayoutNode items[3] = {};
    uint32_t runs[3] = {};
    uint32_t others = 0;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(16)};
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        trigger = prKitButton(kit, row, {.text = "File"}, {});
        if (prKitActivated(kit, trigger))
            prKitToggleOpen(kit, trigger);
        popover = prKitPopover(kit, trigger, {}, {.offset = PrVec2{4, 2}, .minWidth = 240});
        if (popover != kPrLayoutNodeNone) {
            PrLayoutNode menu = prKitMenu(kit, popover, {}, {.minWidth = 200});
            prKitPlace(kit, menu, {.margin = prKitEdgesAll(4)});
            prKitMenuLabel(kit, menu, {.text = "SCENE"});
            PrKitMenuItem create{.text = "New Scene", .shortcut = "Cmd N", .icon = PrKitIconKind_Plus};
            items[0] = prKitMenuItem(kit, menu, create, {});
            prKitMenuSeparator(kit, menu, {});
            items[1] = prKitMenuItem(kit, menu, {.text = "Grid", .checked = true}, {});
            PrKitMenuItemStyle danger{.variant = PrKitMenuItemVariant_Danger};
            items[2] = prKitMenuItem(kit, menu, {.text = "Delete", .disabled = true}, danger);
            for (uint32_t i = 0; i < 3; ++i)
                runs[i] += prKitActivated(kit, items[i]) ? 1 : 0;
        }
        other = prKitButton(kit, row, {.text = "Other"}, {});
        others += prKitActivated(kit, other) ? 1 : 0;
    };
    pair.frame();
    CHECK(pair.alike());
    CHECK(popover == kPrLayoutNodeNone);
    Array<uint8_t> closed;
    REQUIRE(pair.direct.render(&closed));

    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    REQUIRE(popover != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 1);
    CHECK(pair.scripted.number("last(popover, 'toggle').value") == 1);
    CHECK(pair.alike());
    CHECK(pair.focusAlike());
    // The open popover is among the pixels that were compared.
    CHECK(rendersIn(pair.direct, closed, pair.direct.bounds(popover)));
    CHECK(rendersIn(pair.scripted.fixture, closed, pair.direct.bounds(popover)));

    // A disabled item takes no click, and leaves the popover open.
    pair.click(pair.direct.center(items[2]));
    pair.frame(3);
    CHECK(pair.scripted.number("heard(remove, 'click')") == 0);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 1);
    CHECK(pair.alike());

    // An item's click reaches the script before the news that the click closed the popover, which the round that routes
    // the click tells too.
    pair.click(pair.direct.center(items[1]));
    pair.clickRounds();
    CHECK(runs[1] == 1);
    CHECK(pair.scripted.number("heard(grid, 'click')") == 1);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 2);
    CHECK(pair.scripted.number("last(popover, 'toggle').value") == 0);
    CHECK(pair.scripted.number("log[log.length - 2].type === 'click' && log[log.length - 1].type === 'toggle' ? 1 : 0") == 1);
    pair.frame();
    CHECK(popover == kPrLayoutNodeNone);
    pair.frame(30);
    CHECK(pair.alike());

    // A second click on the trigger closes the popover that the first opened.
    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    CHECK(popover != kPrLayoutNodeNone);
    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    CHECK(popover == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 4);
    CHECK(pair.scripted.number("last(popover, 'toggle').value") == 0);

    // A press on another button closes the popover and reaches the button.
    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    pair.click(pair.direct.center(other));
    pair.frame();
    CHECK(others == 1);
    CHECK(pair.scripted.number("heard(other, 'click')") == 1);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 6);
    pair.frame(30);
    CHECK(pair.alike());
}

TEST_CASE("a popover follows the `open` that the script sets, and tells the script when Escape or a press outside closes it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', padding: '16'});
        var trigger = make('button', row, {}, 'Layers');
        var popover = hear(make('popover', trigger, {width: '260'}), 'toggle');
        var menu = make('menu', popover, {'min-width': '120%'});
        make('menu-item', menu, {shortcut: 'S'}, 'Shadows');
        make('menu-item', menu, {}, 'Gizmos');
    )JS");
    PrLayoutNode trigger = kPrLayoutNodeNone;
    PrLayoutNode popover = kPrLayoutNodeNone;
    bool shows = true;
    bool applies = false;
    bool open = false;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {.direction = PrKitDirection_Row, .padding = prKitEdgesAll(16)});
        trigger = prKitButton(kit, row, {.text = "Layers"}, {});
        popover = kPrLayoutNodeNone;
        if (!shows)
            return;
        if (applies)
            prKitSetOpen(kit, trigger, open);
        applies = false;
        popover = prKitPopover(kit, trigger, {}, {});
        if (popover != kPrLayoutNodeNone) {
            prKitPlace(kit, popover, {.width = prKitPx(260)});
            PrLayoutNode menu = prKitMenu(kit, popover, {}, {});
            prKitPlace(kit, menu, {.minWidth = prKitPercent(120)});
            prKitMenuItem(kit, menu, {.text = "Shadows", .shortcut = "S"}, {});
            prKitMenuItem(kit, menu, {.text = "Gizmos"}, {});
        }
    };
    // Sets `open` in the script and opens or closes the trigger with the kit's own call.
    auto set = [&](bool value) {
        pair.scripted.run(value ? "kitTree.set(popover, 'open', '');" : "kitTree.set(popover, 'open', null);");
        applies = true;
        open = value;
    };
    pair.frame();
    CHECK(pair.alike());

    set(true);
    pair.frame(3);
    REQUIRE(popover != kPrLayoutNodeNone);
    CHECK(pair.alike());
    CHECK(pair.focusAlike());
    // The script opened the popover, so the popover tells it nothing.
    CHECK(pair.scripted.number("log.length") == 0);

    pair.key(PrAppKey_Escape);
    pair.frame();
    CHECK(popover == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 1);
    CHECK(pair.scripted.number("last(popover, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(pair.alike());

    set(true);
    pair.frame(3);
    REQUIRE(popover != kPrLayoutNodeNone);
    pair.click(PrVec2{600, 500});
    pair.frame();
    CHECK(popover == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 2);
    pair.frame(30);
    CHECK(pair.alike());

    set(true);
    pair.frame(3);
    set(false);
    pair.frame(3);
    CHECK(popover == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 2);
    CHECK(pair.alike());

    // A popover that leaves the tree while it is open is closed when it returns, and says so.
    set(true);
    pair.frame(3);
    pair.scripted.run("kitTree.remove(popover);");
    shows = false;
    pair.frame(2);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.insert(trigger, popover, 0);");
    shows = true;
    pair.frame(3);
    CHECK(popover == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(popover, 'toggle')") == 3);
    CHECK(pair.scripted.number("last(popover, 'toggle').value") == 0);
    CHECK(pair.alike());
}

TEST_CASE("a menu and a context menu that leave focus where it is open without taking it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '16'});
        var trigger = make('button', row, {}, 'Suggest');
        var popover = make('popover', trigger, {triggered: ''});
        var menu = make('menu', popover, {'leaves-focus': ''});
        make('menu-item', menu, {}, 'First');
        make('menu-item', menu, {}, 'Second');
        var host = make('box', row, {width: '200', height: '120'});
        var context = hear(make('context-menu', host, {'leaves-focus': ''}), 'toggle');
        make('menu-item', context, {}, 'Rename');
    )JS");
    PrLayoutNode trigger = kPrLayoutNodeNone;
    PrLayoutNode host = kPrLayoutNodeNone;
    PrLayoutNode popover = kPrLayoutNodeNone;
    PrLayoutNode context = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(16)};
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        trigger = prKitButton(kit, row, {.text = "Suggest"}, {});
        if (prKitActivated(kit, trigger))
            prKitToggleOpen(kit, trigger);
        popover = prKitPopover(kit, trigger, {}, {});
        if (popover != kPrLayoutNodeNone) {
            PrLayoutNode menu = prKitMenu(kit, popover, {}, {.leavesFocus = true});
            prKitMenuItem(kit, menu, {.text = "First"}, {});
            prKitMenuItem(kit, menu, {.text = "Second"}, {});
        }
        host = prKitBox(kit, row, {}, {.place = {.width = prKitPx(200), .height = prKitPx(120)}});
        context = prKitContextMenu(kit, host, {}, {.leavesFocus = true});
        if (context != kPrLayoutNodeNone)
            prKitMenuItem(kit, context, {.text = "Rename"}, {});
    };
    pair.frame();
    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    REQUIRE(popover != kPrLayoutNodeNone);
    CHECK(prUiFocus(pair.direct.ui) == trigger);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    CHECK(popover == kPrLayoutNodeNone);

    pair.secondaryClick(pair.direct.center(host));
    pair.frame(3);
    REQUIRE(context != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("last(context, 'toggle').value") == 1);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
}

TEST_CASE("a select opens its menu on a click, closes it on an option's click, and reports Up and Down as steps") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {align: 'start', gap: '12', padding: '16'});
        var names = ['Transform', 'Collider', 'Camera'];
        var chosen = 1;
        var select = make('select', page, {icon: 'cube', size: 'sm', width: '220'}, names[chosen]);
        var menu = hear(make('select-menu', select, {margin: '2'}), 'toggle');
        var options = names.map(function (name, i) {
            const option = make('menu-item', menu, i === chosen ? {checked: ''} : {}, name);
            return hear(option, 'click', function () { choose(i); });
        });
        function choose(i) {
            if (i < 0 || i >= names.length)
                return;
            chosen = i;
            kitTree.setText(select, names[i]);
            options.forEach(function (option, j) { kitTree.set(option, 'checked', j === i ? '' : null); });
        }
        hear(select, 'step', function (detail) { choose(chosen + detail.value); });
        make('select', page, {disabled: ''}, 'Disabled');
    )JS");
    PrLayoutNode field = kPrLayoutNodeNone;
    PrLayoutNode options[3] = {};
    uint32_t chosen = 1;
    bool applies = false;
    bool open = false;
    bool listed = false;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        const PrStringView names[] = {"Transform", "Collider", "Camera"};
        PrLayoutNode page =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 12, .padding = prKitEdgesAll(16)});
        PrKitSelect content{.label = names[chosen], .icon = PrKitIconKind_Cube};
        field = prKitSelect(kit, page, content, {.size = PrKitControlSize_Sm});
        prKitPlace(kit, field, {.width = prKitPx(220)});
        int32_t step = 0;
        if (prKitStepped(kit, field, &step) && int32_t(chosen) + step >= 0 && int32_t(chosen) + step < 3)
            chosen = uint32_t(int32_t(chosen) + step);
        if (applies)
            prKitSetOpen(kit, field, open);
        applies = false;
        PrLayoutNode menu = prKitSelectMenu(kit, field);
        listed = menu != kPrLayoutNodeNone;
        if (listed)
            prKitPlace(kit, menu, {.margin = prKitEdgesAll(2)});
        for (uint32_t i = 0; i < 3 && listed; ++i) {
            options[i] = prKitMenuItem(kit, menu, {.text = names[i], .checked = chosen == i}, {});
            if (prKitActivated(kit, options[i]))
                chosen = i;
        }
        prKitSelect(kit, page, {.label = "Disabled", .disabled = true}, {});
    };
    pair.frame();
    CHECK(pair.alike());
    Array<uint8_t> closed;
    REQUIRE(pair.direct.render(&closed));

    pair.click(pair.direct.center(field));
    pair.frame(3);
    REQUIRE(listed);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 1);
    CHECK(pair.scripted.number("last(menu, 'toggle').value") == 1);
    CHECK(pair.alike());
    CHECK(rendersIn(pair.scripted.fixture, closed, pair.direct.bounds(options[2])));

    pair.click(pair.direct.center(options[2]));
    pair.frame();
    CHECK(chosen == 2);
    CHECK(pair.scripted.number("chosen") == 2);
    CHECK_FALSE(listed);
    CHECK(pair.scripted.number("heard(options[2], 'click')") == 1);
    CHECK(pair.scripted.number("last(menu, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(pair.alike());

    // The choice gave the field focus, and with its menu closed, Up and Down step the choice.
    pair.key(PrAppKey_Up);
    pair.frame();
    CHECK(chosen == 1);
    CHECK(pair.scripted.number("chosen") == 1);
    CHECK(pair.scripted.number("last(select, 'step').value") == -1);
    pair.key(PrAppKey_Down);
    pair.frame();
    CHECK(pair.scripted.number("last(select, 'step').value") == 1);
    pair.key(PrAppKey_Down);
    pair.frame();
    CHECK(chosen == 2);
    CHECK(pair.scripted.number("chosen") == 2);
    CHECK(pair.scripted.number("heard(select, 'step')") == 3);
    pair.frame(30);
    CHECK(pair.alike());

    // The script opens and closes the menu, and hears of neither.
    pair.scripted.run("kitTree.set(menu, 'open', '');");
    applies = true;
    open = true;
    pair.frame(30);
    CHECK(listed);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.set(menu, 'open', null);");
    applies = true;
    open = false;
    // The select was declared as open before its menu closed, and the binding asks for the frame that shows it closed.
    pair.frame();
    CHECK(prUiOutput(pair.scripted.fixture.ui).wantsFrame);
    pair.frame(30);
    CHECK_FALSE(listed);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 2);
    CHECK(pair.alike());
}

TEST_CASE("a menu bar's items open the popovers under them, and pass the open menu to the item that the pointer moves onto") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var bar = make('menu-bar', 0, {});
        var names = ['File', 'Edit', 'View', 'Help'];
        var popovers = names.map(function (name, i) {
            const item = make('menu-bar-item', bar, i === 2 ? {disabled: ''} : {}, name);
            const popover = hear(make('popover', item, i === 0 ? {triggered: ''} : {}), 'toggle');
            make('menu-item', make('menu', popover, {}), {}, name + ' item');
            return popover;
        });
        var plain = make('menu-bar', 0, {plain: ''});
        make('menu-bar-item', plain, {}, 'Window');
    )JS");
    PrLayoutNode items[4] = {};
    bool open[4] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode bar = prKitMenuBar(kit, root, {});
        const char* const names[] = {"File", "Edit", "View", "Help"};
        const char* const entries[] = {"File item", "Edit item", "View item", "Help item"};
        for (uint32_t i = 0; i < 4; ++i) {
            items[i] = prKitMenuBarItem(kit, bar, {.text = names[i], .disabled = i == 2});
            PrLayoutNode popover = prKitPopover(kit, items[i], {}, {});
            if (popover != kPrLayoutNodeNone)
                prKitMenuItem(kit, prKitMenu(kit, popover, {}, {}), {.text = entries[i]}, {});
            open[i] = prKitIsOpen(kit, items[i]);
        }
        PrLayoutNode plain = prKitMenuBar(kit, root, {.plain = true});
        prKitMenuBarItem(kit, plain, {.text = "Window"});
    };
    pair.frame();
    CHECK(pair.alike());

    // A disabled item takes no click.
    pair.click(pair.direct.center(items[2]));
    pair.frame(3);
    CHECK_FALSE(open[2]);
    CHECK(pair.scripted.number("log.length") == 0);
    CHECK(pair.alike());

    // The item opens the popover itself, which a popover that is triggered does not undo.
    pair.click(pair.direct.center(items[0]));
    pair.frame(3);
    REQUIRE(open[0]);
    CHECK(pair.scripted.number("heard(popovers[0], 'toggle')") == 1);
    CHECK(pair.scripted.number("last(popovers[0], 'toggle').value") == 1);
    CHECK(pair.alike());

    pair.move(pair.direct.center(items[1]));
    pair.frame(3);
    CHECK_FALSE(open[0]);
    CHECK(open[1]);
    CHECK(pair.scripted.number("last(popovers[0], 'toggle').value") == 0);
    CHECK(pair.scripted.number("last(popovers[1], 'toggle').value") == 1);
    CHECK(pair.alike());

    pair.move(pair.direct.center(items[3]));
    pair.frame(3);
    CHECK(open[3]);
    CHECK(pair.scripted.number("last(popovers[1], 'toggle').value") == 0);
    CHECK(pair.scripted.number("last(popovers[3], 'toggle').value") == 1);
    CHECK(pair.scripted.number("log.length") == 5);
    pair.frame(30);
    CHECK(pair.alike());
}

TEST_CASE("a context menu opens at a secondary click, or where the script opens it, and tells the script what the kit did") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var host = make('box', 0, {flex: 'fill'});
        make('label', host, {}, 'RIGHT-CLICK ANYWHERE');
        var menu = make('context-menu', host, {'min-width': '220', margin: '6'});
        // The script keeps `open` as each toggle says, which leaves the menu where the click opened it.
        hear(menu, 'toggle', function (detail) { kitTree.set(menu, 'open', detail.value ? '' : null); });
        make('menu-label', menu, {}, 'EDIT');
        var rename = hear(make('menu-item', menu, {shortcut: 'F2'}, 'Rename'), 'click');
        var remove = hear(make('menu-item', menu, {variant: 'danger'}, 'Delete'), 'click');
    )JS");
    PrLayoutNode menu = kPrLayoutNodeNone;
    PrLayoutNode items[2] = {};
    uint32_t deletes = 0;
    bool applies = false;
    bool open = false;
    PrVec2 at{};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode host = prKitBox(kit, root, {}, {.place = {.flex = PrKitFlex_Fill}});
        prKitLabel(kit, host, {.text = "RIGHT-CLICK ANYWHERE"}, {});
        if (applies && open)
            prKitOpenContextMenu(kit, host, at);
        else if (applies)
            prKitCloseContextMenu(kit, host);
        applies = false;
        menu = prKitContextMenu(kit, host, {}, {.minWidth = 220});
        if (menu == kPrLayoutNodeNone)
            return;
        prKitPlace(kit, menu, {.margin = prKitEdgesAll(6)});
        prKitMenuLabel(kit, menu, {.text = "EDIT"});
        items[0] = prKitMenuItem(kit, menu, {.text = "Rename", .shortcut = "F2"}, {});
        items[1] = prKitMenuItem(kit, menu, {.text = "Delete"}, {.variant = PrKitMenuItemVariant_Danger});
        deletes += prKitActivated(kit, items[1]) ? 1 : 0;
    };
    pair.frame(2);
    CHECK(pair.alike());
    Array<uint8_t> closed;
    REQUIRE(pair.direct.render(&closed));

    pair.secondaryClick(PrVec2{320, 240});
    pair.frame(3);
    REQUIRE(menu != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 1);
    CHECK(pair.scripted.number("last(menu, 'toggle').values.length") == 3);
    CHECK(pair.scripted.number("last(menu, 'toggle').values[0]") == 1);
    CHECK(pair.scripted.number("last(menu, 'toggle').values[1]") == 320);
    CHECK(pair.scripted.number("last(menu, 'toggle').values[2]") == 240);
    CHECK(pair.alike());
    CHECK(rendersIn(pair.scripted.fixture, closed, pair.direct.bounds(menu)));

    pair.click(pair.direct.center(items[1]));
    pair.frame();
    CHECK(deletes == 1);
    CHECK(menu == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(remove, 'click')") == 1);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 2);
    CHECK(pair.scripted.number("last(menu, 'toggle').values.length") == 1);
    CHECK(pair.scripted.number("last(menu, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(pair.alike());

    // The script opens the menu at a point, and hears of nothing until Escape closes it.
    pair.scripted.run("kitTree.set(menu, 'at', '100 80'); kitTree.set(menu, 'open', '');");
    applies = true;
    open = true;
    at = PrVec2{100, 80};
    pair.frame(3);
    REQUIRE(menu != kPrLayoutNodeNone);
    CHECK(pair.direct.bounds(menu).left == 107.0f);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 2);
    CHECK(pair.alike());
    pair.key(PrAppKey_Escape);
    pair.frame();
    CHECK(menu == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 3);
    pair.frame(30);
    CHECK(pair.alike());

    // The script closes the menu that it opened.
    pair.scripted.run("kitTree.set(menu, 'at', '40 300'); kitTree.set(menu, 'open', '');");
    applies = true;
    open = true;
    at = PrVec2{40, 300};
    pair.frame(3);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.set(menu, 'open', null);");
    applies = true;
    open = false;
    pair.frame(3);
    CHECK(menu == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(menu, 'toggle')") == 3);
    CHECK(pair.alike());
}

TEST_CASE("a tooltip shows under its anchor once the pointer has rested on it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '16'});
        var grid = make('button', row, {variant: 'ghost'}, 'Grid');
        make('tooltip', grid, {shortcut: 'G'}, 'Show Grid');
        var snap = make('button', row, {}, 'Snap');
        make('tooltip', snap, {}, 'Snap to Grid');
    )JS");
    PrLayoutNode anchors[2] = {};
    PrLayoutNode tips[2] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(16)};
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        anchors[0] = prKitButton(kit, row, {.text = "Grid"}, {.variant = PrKitButtonVariant_Ghost});
        tips[0] = prKitTooltip(kit, anchors[0], {.text = "Show Grid", .shortcut = "G"});
        anchors[1] = prKitButton(kit, row, {.text = "Snap"}, {});
        tips[1] = prKitTooltip(kit, anchors[1], {.text = "Snap to Grid"});
    };
    pair.frame();
    Array<uint8_t> hidden;
    REQUIRE(pair.direct.render(&hidden));
    pair.move(pair.direct.center(anchors[0]));
    pair.frame(1, 0.1);
    pair.frame(1, 0.2);
    CHECK(tips[0] == kPrLayoutNodeNone);
    CHECK(pair.alike());
    pair.frame(1, 0.4);
    pair.frame();
    REQUIRE(tips[0] != kPrLayoutNodeNone);
    CHECK(pair.alike());
    CHECK(rendersIn(pair.scripted.fixture, hidden, pair.direct.bounds(tips[0])));

    pair.move(pair.direct.center(anchors[1]));
    pair.frame(1, 0.1);
    pair.frame(1, 0.3);
    pair.frame(1, 0.3);
    pair.frame();
    CHECK(tips[0] == kPrLayoutNodeNone);
    REQUIRE(tips[1] != kPrLayoutNodeNone);
    CHECK(pair.alike());
    pair.move(PrVec2{700, 500});
    pair.frame(30);
    CHECK(tips[1] == kPrLayoutNodeNone);
    CHECK(pair.alike());
}

TEST_CASE("a drag's ghost follows the pointer while a tab is dragged, and is gone when the drag ends") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var strip = make('tab-strip', 0, {reorderable: ''});
        ['Console', 'Output'].forEach(function (label, i) {
            make('tab-label', make('tab', strip, {id: String(i + 1)}), {}, label);
        });
        make('drag-ghost', 0, {icon: 'cube'}, 'player_base.mesh');
    )JS");
    TwoTabs held;
    PrLayoutNode first = kPrLayoutNodeNone;
    PrLayoutNode ghost = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode strip = prKitTabStrip(kit, root, {.tabs = held.tabs, .reorderable = true}, {});
        first = prKitTab(kit, strip, {.id = 1});
        prKitTabLabel(kit, first, {.text = "Console"});
        prKitTabLabel(kit, prKitTab(kit, strip, {.id = 2}), {.text = "Output"});
        ghost = prKitDragGhost(kit, {.text = "player_base.mesh", .icon = PrKitIconKind_Cube});
    };
    pair.frame();
    CHECK(ghost == kPrLayoutNodeNone);
    CHECK(pair.alike());
    Array<uint8_t> still;
    REQUIRE(pair.direct.render(&still));

    PrVec2 start = pair.direct.center(first);
    PrVec2 away{start.x + 100, start.y + 100};
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = start}});
    pair.frame();
    pair.move(away);
    pair.frame(2);
    REQUIRE(ghost != kPrLayoutNodeNone);
    CHECK(pair.alike());
    CHECK(rendersIn(pair.scripted.fixture, still, pair.direct.bounds(ghost)));
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = away}});
    pair.frame(30);
    CHECK(ghost == kPrLayoutNodeNone);
    CHECK(pair.alike());
}

TEST_CASE("a modal opens over a scrim with focus on the node that its autofocus names, and closes on Escape or a press on the scrim") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', padding: '16'});
        var trigger = make('button', row, {}, 'Open');
        var attributes = {triggered: '', placement: 'top', inset: '80', 'min-width': '300'};
        var modal = hear(make('modal', trigger, attributes), 'toggle');
        var panel = make('box', modal, {direction: 'row', gap: '8', padding: '24'});
        var cancel = hear(make('button', panel, {}, 'Cancel'), 'click');
        var ok = hear(make('button', panel, {variant: 'primary'}, 'OK'), 'click');
        make('modal-autofocus', ok, {});
    )JS");
    PrLayoutNode trigger = kPrLayoutNodeNone;
    PrLayoutNode content = kPrLayoutNodeNone;
    PrLayoutNode ok = kPrLayoutNodeNone;
    uint32_t oks = 0;
    bool applies = false;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {.direction = PrKitDirection_Row, .padding = prKitEdgesAll(16)});
        trigger = prKitButton(kit, row, {.text = "Open"}, {});
        if (applies || prKitActivated(kit, trigger))
            prKitSetOpen(kit, trigger, true);
        applies = false;
        content = prKitModal(kit, trigger, {}, {.placement = PrKitModalPlacement_Top, .inset = 80});
        if (content == kPrLayoutNodeNone)
            return;
        prKitPlace(kit, content, {.minWidth = prKitPx(300)});
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(24)};
        PrLayoutNode panel = prKitBox(kit, content, {}, line);
        prKitButton(kit, panel, {.text = "Cancel"}, {});
        ok = prKitButton(kit, panel, {.text = "OK"}, {.variant = PrKitButtonVariant_Primary});
        prKitModalAutofocus(kit, ok);
        oks += prKitActivated(kit, ok) ? 1 : 0;
    };
    pair.frame();
    CHECK(pair.alike());
    Array<uint8_t> closed;
    REQUIRE(pair.direct.render(&closed));

    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    REQUIRE(content != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("last(modal, 'toggle').value") == 1);
    CHECK(prUiFocus(pair.direct.ui) == ok);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
    // The scrim covers the viewport in both, far from the content.
    CHECK(rendersIn(pair.scripted.fixture, closed, PrRect{700, 500, 720, 520}));
    CHECK(pair.direct.bounds(content).top == 80.0f);

    // Enter activates the button that has focus.
    pair.key(PrAppKey_Enter);
    pair.frame();
    CHECK(oks == 1);
    CHECK(pair.scripted.number("heard(ok, 'click')") == 1);
    CHECK(pair.scripted.number("heard(cancel, 'click')") == 0);
    pair.key(PrAppKey_Escape);
    pair.frame();
    CHECK(content == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(modal, 'toggle')") == 2);
    CHECK(pair.scripted.number("last(modal, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(pair.alike());

    // The script opens the modal. The trigger is under the scrim, so a press on it closes the modal and opens none.
    pair.scripted.run("kitTree.set(modal, 'open', '');");
    applies = true;
    pair.frame(3);
    REQUIRE(content != kPrLayoutNodeNone);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
    pair.click(pair.direct.center(trigger));
    pair.frame();
    CHECK(content == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(modal, 'toggle')") == 3);
    pair.frame(30);
    CHECK(pair.alike());
}

TEST_CASE("a dialog shows its title over its body and footer, and its close button and the script close it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var trigger = make('button', 0, {}, 'Rename');
        var attributes = {triggered: '', width: '360', 'min-height': '120', placement: 'top', inset: '40'};
        var dialog = hear(make('dialog', trigger, attributes, 'Rename Entity'), 'toggle');
        make('label', dialog, {}, 'Pick a name');
        var footer = make('dialog-footer', dialog, {});
        var cancel = hear(make('button', footer, {}, 'Cancel'), 'click', function () {
            kitTree.set(dialog, 'open', null);
        });
        make('button', footer, {variant: 'primary'}, 'OK');
        var plain = make('button', 0, {}, 'About');
        var about = hear(make('dialog', plain, {triggered: ''}, 'About Studio'), 'toggle');
    )JS");
    PrLayoutNode triggers[2] = {};
    PrLayoutNode bodies[2] = {};
    PrLayoutNode cancel = kPrLayoutNodeNone;
    bool applies = false;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        triggers[0] = prKitButton(kit, root, {.text = "Rename"}, {});
        if (prKitActivated(kit, triggers[0]))
            prKitSetOpen(kit, triggers[0], true);
        PrKitDialogStyle style{.modal = {.placement = PrKitModalPlacement_Top, .inset = 40}, .width = 360};
        bodies[0] = prKitDialog(kit, triggers[0], {.title = "Rename Entity"}, style);
        if (bodies[0] != kPrLayoutNodeNone) {
            prKitPlace(kit, bodies[0], {.minHeight = prKitPx(120)});
            prKitLabel(kit, bodies[0], {.text = "Pick a name"}, {});
            PrLayoutNode footer = prKitDialogFooter(kit, bodies[0]);
            cancel = prKitButton(kit, footer, {.text = "Cancel"}, {});
            if (prKitActivated(kit, cancel))
                prKitSetOpen(kit, triggers[0], false);
            prKitButton(kit, footer, {.text = "OK"}, {.variant = PrKitButtonVariant_Primary});
        }
        triggers[1] = prKitButton(kit, root, {.text = "About"}, {});
        if (applies || prKitActivated(kit, triggers[1]))
            prKitSetOpen(kit, triggers[1], true);
        applies = false;
        bodies[1] = prKitDialog(kit, triggers[1], {.title = "About Studio"}, {});
    };
    pair.frame();
    CHECK(pair.alike());

    pair.click(pair.direct.center(triggers[0]));
    pair.frame(3);
    REQUIRE(bodies[0] != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("last(dialog, 'toggle').value") == 1);
    CHECK(pair.direct.bounds(bodies[0]).right - pair.direct.bounds(bodies[0]).left == doctest::Approx(358.0));
    CHECK(pair.focusAlike());
    CHECK(pair.alike());

    // The close button is the dialog's first focusable node, and the modal moved focus onto it.
    PrLayoutNode close = prUiFocus(pair.direct.ui);
    REQUIRE(close != kPrLayoutNodeNone);
    pair.click(pair.direct.center(close));
    pair.frame();
    CHECK(bodies[0] == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(dialog, 'toggle')") == 2);
    CHECK(pair.scripted.number("last(dialog, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(pair.alike());

    // The script closes the dialog on its Cancel button, and hears of no close.
    pair.click(pair.direct.center(triggers[0]));
    pair.frame(3);
    REQUIRE(bodies[0] != kPrLayoutNodeNone);
    pair.click(pair.direct.center(cancel));
    pair.frame();
    CHECK(bodies[0] == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(cancel, 'click')") == 1);
    CHECK(pair.scripted.number("heard(dialog, 'toggle')") == 3);
    pair.frame(30);
    CHECK(pair.alike());

    // A dialog with no attributes but its trigger is centered, at the kit's width. Escape closes it, and the script
    // opens it again.
    pair.click(pair.direct.center(triggers[1]));
    pair.frame(3);
    REQUIRE(bodies[1] != kPrLayoutNodeNone);
    CHECK(pair.alike());
    pair.key(PrAppKey_Escape);
    pair.frame();
    CHECK(bodies[1] == kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(about, 'toggle')") == 2);
    pair.scripted.run("kitTree.set(about, 'open', '');");
    applies = true;
    pair.frame(3);
    REQUIRE(bodies[1] != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("heard(about, 'toggle')") == 2);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
}

TEST_CASE("a command palette lists its items, tells the script the query that the user types, and runs the selected item") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var trigger = make('button', 0, {}, 'Search');
        var attributes = {triggered: '', prompt: 'Type a command', searchable: ''};
        var palette = hear(make('command-palette', trigger, attributes), 'toggle');
        var commands = [
            {text: 'New Scene', attributes: {category: 'File', shortcut: 'Cmd N', icon: 'plus'}},
            {text: 'Save', attributes: {category: 'File', shortcut: 'Cmd S'}},
            {text: 'Save All', attributes: {category: 'File', disabled: ''}},
            {text: 'Build and Run', attributes: {category: 'Build', icon: 'play'}},
        ];
        var ran = -1;
        var runs = 0;
        var items = commands.map(function (command, i) {
            return hear(make('command-item', palette, command.attributes, command.text), 'click', function () {
                ran = i;
                runs += 1;
            });
        });
        var footer = make('command-palette-footer', palette, {});
        var query = '';
        function filter(text) {
            query = text;
            commands.forEach(function (command, i) {
                if (command.text.toLowerCase().indexOf(text.toLowerCase()) < 0)
                    kitTree.remove(items[i]);
                else
                    kitTree.insert(palette, items[i], footer);
            });
        }
        hear(palette, 'change', function (detail) { filter(detail.text); });
    )JS");
    struct Command {
        PrStringView text;
        PrStringView category;
        PrStringView shortcut;
        PrKitIconKind icon;
        bool disabled;
    };
    const Command commands[] = {
        {"New Scene", "File", "Cmd N", PrKitIconKind_Plus, false},
        {"Save", "File", "Cmd S", PrKitIconKind_None, false},
        {"Save All", "File", "", PrKitIconKind_None, true},
        {"Build and Run", "Build", "", PrKitIconKind_Play, false},
    };
    PrLayoutNode trigger = kPrLayoutNodeNone;
    PrLayoutNode list = kPrLayoutNodeNone;
    String query;
    uint32_t ran = 4;
    uint32_t runs = 0;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        trigger = prKitButton(kit, root, {.text = "Search"}, {});
        if (prKitActivated(kit, trigger))
            prKitSetOpen(kit, trigger, true);
        uint32_t shown[4] = {};
        uint32_t count = 0;
        for (uint32_t i = 0; i < 4; ++i) {
            if (holds(commands[i].text, query))
                shown[count++] = i;
        }
        list = prKitCommandPalette(kit, trigger, {
            .prompt = "Type a command",
            .count = count,
            .query = query,
            .searchable = true,
        });
        if (list == kPrLayoutNodeNone)
            return;
        PrStringView text = {};
        if (prKitTextChanged(kit, list, &text))
            query = text;
        for (uint32_t i = 0; i < count; ++i) {
            const Command& command = commands[shown[i]];
            PrLayoutNode item = prKitCommandItem(kit, list, {
                .text = command.text,
                .category = command.category,
                .shortcut = command.shortcut,
                .icon = command.icon,
                .disabled = command.disabled,
            });
            if (prKitActivated(kit, item)) {
                ran = shown[i];
                ++runs;
            }
        }
        prKitCommandPaletteFooter(kit, list);
    };
    pair.frame();
    CHECK(pair.alike());

    pair.click(pair.direct.center(trigger));
    pair.frame(3);
    REQUIRE(list != kPrLayoutNodeNone);
    CHECK(pair.scripted.number("last(palette, 'toggle').value") == 1);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());

    // The search row has focus, and what is typed in it reaches the script, which removes the items that do not match.
    pair.type("sa");
    pair.frame();
    CHECK(query == "sa");
    CHECK(pair.scripted.number("heard(palette, 'change')") == 1);
    CHECK(pair.scripted.number("query === 'sa' ? 1 : 0") == 1);
    pair.frame(3);
    CHECK(pair.alike());

    // The script sets the query, and hears of no change.
    pair.scripted.run("kitTree.set(palette, 'query', 'S'); filter('S');");
    query = "S";
    pair.frame(3);
    CHECK(pair.scripted.number("heard(palette, 'change')") == 1);
    CHECK(pair.alike());

    // Down moves the selection from New Scene to Save, and Enter runs it and closes the palette.
    pair.key(PrAppKey_Down);
    pair.frame();
    CHECK(pair.alike());
    pair.key(PrAppKey_Enter);
    pair.frame();
    CHECK(runs == 1);
    CHECK(ran == 1);
    CHECK(pair.scripted.number("runs") == 1);
    CHECK(pair.scripted.number("ran") == 1);
    CHECK(pair.scripted.number("heard(palette, 'toggle')") == 2);
    CHECK(pair.scripted.number("last(palette, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(list == kPrLayoutNodeNone);
    CHECK(pair.alike());
}

TEST_CASE("a command palette that is not searchable shows its prompt, and runs the item that is clicked") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var palette = hear(make('command-palette', 0, {prompt: 'Run a command', 'max-height': '60'}), 'toggle');
        var items = ['Save', 'Build', 'Deploy'].map(function (text) {
            return hear(make('command-item', palette, {}, text), 'click');
        });
        kitTree.set(palette, 'open', '');
    )JS");
    PrLayoutNode list = kPrLayoutNodeNone;
    PrLayoutNode items[3] = {};
    uint32_t runs = 0;
    bool applies = true;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        if (applies)
            prKitSetOpen(kit, root, true);
        applies = false;
        list = prKitCommandPalette(kit, root, {.prompt = "Run a command", .count = 3});
        if (list == kPrLayoutNodeNone)
            return;
        prKitPlace(kit, list, {.maxHeight = prKitPx(60)});
        const char* const names[] = {"Save", "Build", "Deploy"};
        for (uint32_t i = 0; i < 3; ++i) {
            items[i] = prKitCommandItem(kit, list, {.text = names[i]});
            runs += prKitActivated(kit, items[i]) ? 1 : 0;
        }
    };
    pair.frame(3);
    REQUIRE(list != kPrLayoutNodeNone);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
    // Down twice brings the last item into the list's view, which is too short for the three.
    pair.key(PrAppKey_Down);
    pair.frame();
    pair.key(PrAppKey_Down);
    pair.frame(3);
    CHECK(pair.alike());
    pair.click(pair.direct.center(items[2]));
    pair.frame();
    CHECK(runs == 1);
    CHECK(pair.scripted.number("heard(items[2], 'click')") == 1);
    CHECK(pair.scripted.number("heard(palette, 'toggle')") == 1);
    CHECK(pair.scripted.number("last(palette, 'toggle').value") == 0);
    pair.frame(30);
    CHECK(list == kPrLayoutNodeNone);
    CHECK(pair.alike());
}

TEST_CASE("the menus and overlays take only the values of their attributes, and a refused value changes nothing") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '16'});
        var trigger = make('button', row, {}, 'File');
        var popover = make('popover', trigger, {offset: '0 6', open: ''});
        var menu = make('menu', popover, {});
        var label = make('menu-label', menu, {}, 'SCENE');
        var item = make('menu-item', menu, {shortcut: 'Cmd N', variant: 'danger'}, 'New Scene');
        var separator = make('menu-separator', menu, {});
        var select = make('select', row, {size: 'lg'}, 'Collider');
        var options = make('select-menu', select, {});
        var bar = make('menu-bar', row, {plain: ''});
        var entry = make('menu-bar-item', bar, {}, 'Edit');
        var context = make('context-menu', row, {});
        var tooltip = make('tooltip', trigger, {shortcut: 'F'}, 'File');
        var ghost = make('drag-ghost', row, {icon: 'cube'}, 'a payload');
        var modal = make('modal', select, {placement: 'top'});
        var focus = make('modal-autofocus', trigger, {});
        var dialog = make('dialog', entry, {}, 'Rename');
        var footer = make('dialog-footer', row, {});
        var palette = make('command-palette', row, {prompt: 'Search'});
        var command = make('command-item', palette, {category: 'File'}, 'Save');
        var hints = make('command-palette-footer', palette, {});
        var refused = [
            kitTree.set(popover, 'offset', '4'), kitTree.set(popover, 'offset', '4 2 1'),
            kitTree.set(popover, 'offset', 'left top'), kitTree.set(popover, 'open', 'yes'),
            kitTree.set(popover, 'triggered', 'click'), kitTree.set(popover, 'placement', 'top'),
            kitTree.set(menu, 'leaves-focus', '1'), kitTree.set(menu, 'open', ''), kitTree.set(label, 'text', 'x'),
            kitTree.set(item, 'variant', 'loud'), kitTree.set(item, 'icon', 'no-such-icon'),
            kitTree.set(item, 'checked', 'on'), kitTree.set(item, 'selected', ''),
            kitTree.set(separator, 'disabled', ''), kitTree.set(select, 'size', 'tiny'),
            kitTree.set(select, 'direction', 'row'), kitTree.set(select, 'open', ''),
            kitTree.set(options, 'triggered', ''), kitTree.set(options, 'open', 'open'),
            kitTree.set(bar, 'plain', 'very'), kitTree.set(entry, 'plain', ''), kitTree.set(entry, 'disabled', '0'),
            kitTree.set(context, 'at', '100'), kitTree.set(context, 'at', '1 2 3'),
            kitTree.set(context, 'triggered', ''), kitTree.set(context, 'offset', '1 2'),
            kitTree.set(tooltip, 'icon', 'cube'), kitTree.set(ghost, 'icon', 'none'),
            kitTree.set(ghost, 'shortcut', 'G'), kitTree.set(modal, 'placement', 'bottom'),
            kitTree.set(modal, 'inset', '-1'), kitTree.set(modal, 'title', 'x'), kitTree.set(focus, 'open', ''),
            kitTree.set(dialog, 'inset', 'far'), kitTree.set(dialog, 'placement', ''),
            kitTree.set(footer, 'gap', '4'), kitTree.set(palette, 'searchable', 'no'),
            kitTree.set(palette, 'count', '3'), kitTree.set(command, 'disabled', 'x'),
            kitTree.set(command, 'checked', ''), kitTree.set(hints, 'prompt', 'x'),
            kitTree.listen(popover, 'click', true), kitTree.listen(menu, 'toggle', true),
            kitTree.listen(item, 'change', true), kitTree.listen(select, 'toggle', true),
            kitTree.listen(options, 'step', true), kitTree.listen(entry, 'click', true),
            kitTree.listen(tooltip, 'toggle', true), kitTree.listen(ghost, 'drop', true),
            kitTree.listen(dialog, 'change', true), kitTree.listen(command, 'toggle', true),
        ].filter(Boolean).length;
        var taken = [
            kitTree.listen(popover, 'toggle', true), kitTree.listen(item, 'click', true),
            kitTree.listen(select, 'step', true), kitTree.listen(options, 'toggle', true),
            kitTree.listen(context, 'toggle', true), kitTree.listen(modal, 'toggle', true),
            kitTree.listen(dialog, 'toggle', true), kitTree.listen(palette, 'toggle', true),
            kitTree.listen(palette, 'change', true), kitTree.listen(command, 'click', true),
            kitTree.set(popover, 'offset', '-4.5 +2'), kitTree.set(popover, 'offset', ' 0  6 '),
            kitTree.set(context, 'at', '10 20'), kitTree.set(context, 'at', null),
            kitTree.set(popover, 'triggered', 'true'), kitTree.set(popover, 'triggered', 'false'),
        ].filter(Boolean).length;
    )JS");
    CHECK(pair.scripted.number("refused") == 0);
    CHECK(pair.scripted.number("taken") == 16);
    bool applies = true;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(16)};
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        PrLayoutNode trigger = prKitButton(kit, row, {.text = "File"}, {});
        if (applies)
            prKitSetOpen(kit, trigger, true);
        applies = false;
        PrLayoutNode popover = prKitPopover(kit, trigger, {}, {.offset = PrVec2{0, 6}});
        if (popover != kPrLayoutNodeNone) {
            PrLayoutNode menu = prKitMenu(kit, popover, {}, {});
            prKitMenuLabel(kit, menu, {.text = "SCENE"});
            PrKitMenuItemStyle danger{.variant = PrKitMenuItemVariant_Danger};
            prKitMenuItem(kit, menu, {.text = "New Scene", .shortcut = "Cmd N"}, danger);
            prKitMenuSeparator(kit, menu, {});
        }
        prKitTooltip(kit, trigger, {.text = "File", .shortcut = "F"});
        PrLayoutNode select = prKitSelect(kit, row, {.label = "Collider"}, {.size = PrKitControlSize_Lg});
        prKitSelectMenu(kit, select);
        PrLayoutNode bar = prKitMenuBar(kit, row, {.plain = true});
        PrLayoutNode entry = prKitMenuBarItem(kit, bar, {.text = "Edit"});
        prKitDialog(kit, entry, {.title = "Rename"}, {});
        prKitContextMenu(kit, row, {}, {});
        prKitDragGhost(kit, {.text = "a payload", .icon = PrKitIconKind_Cube});
        prKitDialogFooter(kit, row);
        prKitCommandPalette(kit, row, {.prompt = "Search", .count = 1});
    };
    pair.frame(3);
    CHECK(pair.alike());
    CHECK(pair.focusAlike());
}

TEST_CASE("a part of a menu that is not under its container, and an overlay after the first that its trigger opens, are not declared") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '16'});
        var trigger = make('button', row, {}, 'File');
        var first = hear(make('popover', trigger, {open: ''}), 'toggle');
        make('label', first, {}, 'the popover that the button opens');
        var second = hear(make('popover', trigger, {open: ''}), 'toggle');
        make('label', second, {}, 'a second popover');
        var third = hear(make('dialog', trigger, {open: ''}, 'A dialog of the same trigger'), 'toggle');
        make('select-menu', trigger, {open: ''});
        var lone = make('button', row, {}, 'Lone');
        make('menu-item', make('select-menu', lone, {open: ''}), {}, 'an option under a button');
        make('menu-bar-item', row, {}, 'an item under a box');
        make('command-item', row, {}, 'a command under a box');
        make('command-palette-footer', row, {});
        var host = make('box', row, {width: '120', height: '80'});
        var context = hear(make('context-menu', host, {at: '300 200', open: ''}), 'toggle');
        make('menu-item', context, {}, 'Rename');
        var again = hear(make('context-menu', host, {at: '500 200', open: ''}), 'toggle');
        make('menu-item', again, {}, 'a second context menu');
        var label = make('label', row, {}, 'a label, which takes no focus');
        make('modal-autofocus', label, {});
        make('modal-autofocus', row, {});
    )JS");
    bool applies = true;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(16)};
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        PrLayoutNode trigger = prKitButton(kit, row, {.text = "File"}, {});
        if (applies)
            prKitSetOpen(kit, trigger, true);
        PrLayoutNode popover = prKitPopover(kit, trigger, {}, {});
        if (popover != kPrLayoutNodeNone)
            prKitLabel(kit, popover, {.text = "the popover that the button opens"}, {});
        prKitButton(kit, row, {.text = "Lone"}, {});
        PrLayoutNode host = prKitBox(kit, row, {}, {.place = {.width = prKitPx(120), .height = prKitPx(80)}});
        if (applies)
            prKitOpenContextMenu(kit, host, PrVec2{300, 200});
        applies = false;
        PrLayoutNode context = prKitContextMenu(kit, host, {}, {});
        if (context != kPrLayoutNodeNone)
            prKitMenuItem(kit, context, {.text = "Rename"}, {});
        prKitLabel(kit, row, {.text = "a label, which takes no focus"}, {});
    };
    pair.frame(3);
    CHECK(pair.alike());
    CHECK(pair.focusAlike());
    // The overlays that are not declared tell the script nothing, and those that the script opened have nothing to
    // tell.
    CHECK(pair.scripted.number("log.length") == 0);
}

TEST_CASE("a modal's autofocus on a node that takes no focus leaves focus where the modal puts it") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var modal = make('modal', 0, {open: ''});
        var panel = make('box', modal, {gap: '8', padding: '24'});
        make('modal-autofocus', make('label', panel, {}, 'Delete the selected entities?'), {});
        make('button', panel, {}, 'Cancel');
        make('modal-autofocus', make('button', panel, {disabled: ''}, 'Delete'), {});
    )JS");
    bool applies = true;
    PrLayoutNode cancel = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        if (applies)
            prKitSetOpen(kit, root, true);
        applies = false;
        PrLayoutNode content = prKitModal(kit, root, {}, {});
        if (content == kPrLayoutNodeNone)
            return;
        PrLayoutNode panel = prKitBox(kit, content, {}, {.gap = 8, .padding = prKitEdgesAll(24)});
        prKitLabel(kit, panel, {.text = "Delete the selected entities?"}, {});
        cancel = prKitButton(kit, panel, {.text = "Cancel"}, {});
        prKitButton(kit, panel, {.text = "Delete", .disabled = true}, {});
    };
    pair.frame(4);
    CHECK(prUiFocus(pair.direct.ui) == cancel);
    CHECK(pair.focusAlike());
    CHECK(pair.alike());
}
