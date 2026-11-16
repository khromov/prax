// The kinds of Studio Kit's choices, tabs and progress in its QuickJS binding: each declares what the same calls to the
// kit declare.

#include "pr/app.h"
#include "pr/app_backend.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_choice.h"
#include "pr/kit_text.h"

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
    void key(PrAppKey key) {
        scripted.fixture.key(key);
        direct.key(key);
    }

    // Returns true if both render the same pixels, and the script's tree is laid out as a layout from scratch lays it
    // out.
    bool alike() { return kitVerified(scripted.fixture.tree) && jsKitRendersAlike(scripted.fixture, direct); }
};

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

// A window whose only function is the start of a drag, which it counts. A context that is given one (prUiSetWindow)
// hands it the payload that the pointer drags out of the viewport, as it hands a real window's to the system.
struct DragStartWindow : PrAppWindow {
    int starts = 0;
    mutable PrApp app{};

    DragStartWindow() : PrAppWindow{} {
        appFn = [](const PrAppWindow* window) -> PrApp* { return &static_cast<const DragStartWindow*>(window)->app; };
        dragStartFn = [](PrAppWindow* window, const PrAppDragDesc*) { ++static_cast<DragStartWindow*>(window)->starts; };
    }
};

// The kit's tabs of a test's strip, which hold `count` tabs with ids from `first`.
struct TestTabs {
    PrKitTabs* tabs = prKitTabsCreate();
    explicit TestTabs(uint32_t count, uint64_t first = 1) {
        for (uint32_t i = 0; i < count; ++i)
            prKitTabsInsert(tabs, first + i, i);
    }
    TestTabs(const TestTabs&) = delete;
    TestTabs& operator=(const TestTabs&) = delete;
    ~TestTabs() { prKitTabsDestroy(tabs); }
};

// The label of a test's tab, by its id.
PrStringView tabName(uint64_t id) {
    const char* const names[] = {"Console", "Output", "Profiler", "Animator", "Scene", "Game"};
    if (id >= 1 && id <= 6)
        return names[id - 1];
    return id == 10 ? "Scene" : "Game";
}

// Creates the tabs that `tabs` holds under `strip`, each with its label, and writes the node of the tab `id` to
// `nodes[id]`.
void declareTabs(PrKit* kit, PrLayoutNode strip, PrKitTabs* tabs, PrLayoutNode* nodes) {
    for (uint32_t i = 0; i < prKitTabsCount(tabs); ++i) {
        uint64_t id = prKitTabsAt(tabs, i);
        nodes[id] = prKitTab(kit, strip, {.id = id});
        prKitTabLabel(kit, nodes[id], {.text = tabName(id)});
    }
}

// The script that makes a strip's tabs as declareTabs does: `tab(strip, id, label)` makes a tab with a label under a
// strip, keeps its node in `tabs` by its id, and returns it.
constexpr const char* kTabs = R"JS(
    var tabs = {};
    function tab(strip, id, label) {
        tabs[id] = make('tab', strip, {id: String(id)});
        make('tab-label', tabs[id], {}, label);
        return tabs[id];
    }
)JS";

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("segments and radios render as the kit's own, and a click or an arrow key tells the script which was chosen") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {align: 'start', gap: '12', padding: '16'});
        var mode = 0;
        var track = make('segmented', page, {size: 'sm', width: '240'});
        var segments = ['Local', 'World', 'Parent'].map(function (label, i) {
            return hear(make('segment', track, i === mode ? {selected: ''} : {}, label), 'click', function () {
                mode = i;
                segments.forEach(function (segment, j) { kitTree.set(segment, 'selected', j === i ? '' : null); });
            });
        });
        var fixed = hear(make('segment', track, {disabled: ''}, 'Fixed'), 'click');
        var shapes = make('segmented', page, {disabled: ''});
        var cube = hear(make('segment', shapes, {icon: 'cube', selected: ''}), 'click');
        make('segment', shapes, {icon: 'grid'}, 'Grid');
        var shading = 1;
        var group = make('radio-group', page, {direction: 'row', size: 'lg'});
        var radios = ['Unlit', 'Lambert', 'Toon'].map(function (label, i) {
            const attributes = i === 0 ? {icon: 'cube'} : i === shading ? {selected: ''} : {};
            return hear(make('radio', group, attributes, label), 'click', function () {
                shading = i;
                radios.forEach(function (radio, j) { kitTree.set(radio, 'selected', j === i ? '' : null); });
            });
        });
        var column = make('radio-group', page, {});
        make('radio', column, {selected: ''}, 'Mesh');
        var off = hear(make('radio', column, {disabled: ''}, 'Volume'), 'click');
        var locked = make('radio-group', page, {disabled: '', size: 'xs'});
        var held = hear(make('radio', locked, {selected: ''}, 'Held'), 'click');
    )JS");
    uint32_t mode = 0;
    uint32_t shading = 1;
    PrLayoutNode segments[3] = {};
    PrLayoutNode radios[3] = {};
    PrLayoutNode cube = kPrLayoutNodeNone;
    PrLayoutNode off = kPrLayoutNodeNone;
    PrLayoutNode fixed = kPrLayoutNodeNone;
    PrLayoutNode held = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 12, .padding = prKitEdgesAll(16)});
        PrLayoutNode track = prKitSegmented(kit, page, {}, {.size = PrKitControlSize_Sm});
        prKitPlace(kit, track, {.width = prKitPx(240)});
        const char* const modes[] = {"Local", "World", "Parent"};
        for (uint32_t i = 0; i < 3; ++i) {
            segments[i] = prKitSegment(kit, track, {.label = modes[i], .selected = mode == i});
            if (prKitActivated(kit, segments[i]))
                mode = i;
        }
        fixed = prKitSegment(kit, track, {.label = "Fixed", .disabled = true});
        PrLayoutNode shapes = prKitSegmented(kit, page, {.disabled = true}, {});
        cube = prKitSegment(kit, shapes, {.icon = PrKitIconKind_Cube, .selected = true});
        prKitSegment(kit, shapes, {.label = "Grid", .icon = PrKitIconKind_Grid});
        PrKitChoiceStyle row{.size = PrKitControlSize_Lg, .direction = PrKitDirection_Row};
        PrLayoutNode group = prKitRadioGroup(kit, page, {}, row);
        const char* const shadings[] = {"Unlit", "Lambert", "Toon"};
        for (uint32_t i = 0; i < 3; ++i) {
            PrKitChoice choice{.label = shadings[i], .selected = shading == i};
            if (i == 0)
                choice.icon = PrKitIconKind_Cube;
            radios[i] = prKitRadio(kit, group, choice);
            if (prKitActivated(kit, radios[i]))
                shading = i;
        }
        PrLayoutNode column = prKitRadioGroup(kit, page, {}, {});
        prKitRadio(kit, column, {.label = "Mesh", .selected = true});
        off = prKitRadio(kit, column, {.label = "Volume", .disabled = true});
        PrLayoutNode locked = prKitRadioGroup(kit, page, {.disabled = true}, {.size = PrKitControlSize_Xs});
        held = prKitRadio(kit, locked, {.label = "Held", .selected = true});
    };
    pair.frame();
    CHECK(pair.alike());

    // A click chooses a segment, and Right moves focus to the next, which is activated in the round after.
    pair.click(pair.direct.center(segments[1]));
    pair.frame();
    CHECK(mode == 1);
    CHECK(pair.scripted.number("mode") == 1);
    CHECK(pair.scripted.number("heard(segments[1], 'click')") == 1);
    pair.key(PrAppKey_Right);
    pair.frame();
    CHECK(mode == 2);
    CHECK(pair.scripted.number("mode") == 2);
    pair.frame(30);
    CHECK(pair.alike());

    // A row of radios moves by Left and Right.
    pair.click(pair.direct.center(radios[2]));
    pair.frame();
    CHECK(shading == 2);
    CHECK(pair.scripted.number("shading") == 2);
    pair.key(PrAppKey_Left);
    pair.frame();
    CHECK(shading == 1);
    CHECK(pair.scripted.number("shading") == 1);
    CHECK(pair.scripted.number("heard(radios[1], 'click')") == 1);
    pair.frame(30);
    CHECK(pair.alike());

    // A disabled segment or radio, and one of a disabled control or group, take no click.
    const PrLayoutNode disabled[] = {cube, off, fixed, held};
    for (PrLayoutNode node : disabled) {
        pair.click(pair.direct.center(node));
        pair.frame();
    }
    pair.frame(30);
    CHECK(pair.scripted.number("heard(cube, 'click') + heard(off, 'click')") == 0);
    CHECK(pair.scripted.number("heard(fixed, 'click') + heard(held, 'click')") == 0);
    CHECK(pair.scripted.number("log.length") == 4);
    CHECK(pair.alike());
}

TEST_CASE("a checkbox and a toggle flip the value that their node holds, tell the script, and take the value it sets") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {gap: '8', padding: '16'});
        var shadows = hear(make('checkbox', page, {value: '', size: 'sm'}, 'Cast Shadows'), 'change');
        var locked = hear(make('checkbox', page, {disabled: ''}, 'Locked'), 'change');
        var physics = hear(make('toggle', page, {size: 'lg'}, 'Physics'), 'change');
        var snap = hear(make('toggle', page, {value: 'true', disabled: ''}, 'Snap'), 'change');
    )JS");
    bool values[4] = {true, false, false, true};
    PrLayoutNode rows[4] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.gap = 8, .padding = prKitEdgesAll(16)});
        rows[0] =
            prKitCheckbox(kit, page, {.value = &values[0], .label = "Cast Shadows"}, {.size = PrKitControlSize_Sm});
        rows[1] = prKitCheckbox(kit, page, {.value = &values[1], .label = "Locked", .disabled = true}, {});
        rows[2] = prKitToggle(kit, page, {.value = &values[2], .label = "Physics"}, {.size = PrKitControlSize_Lg});
        rows[3] = prKitToggle(kit, page, {.value = &values[3], .label = "Snap", .disabled = true}, {});
    };
    pair.frame();
    CHECK(pair.alike());

    pair.click(pair.direct.center(rows[0]));
    pair.frame();
    CHECK_FALSE(values[0]);
    CHECK(pair.scripted.number("heard(shadows, 'change')") == 1);
    CHECK(pair.scripted.number("last(shadows, 'change').value") == 0);
    pair.click(pair.direct.center(rows[2]));
    pair.frame();
    CHECK(values[2]);
    CHECK(pair.scripted.number("last(physics, 'change').value") == 1);
    // The toggle's knob eases across its track in both.
    pair.frame(3);
    CHECK(pair.alike());
    pair.frame(30);
    CHECK(pair.alike());

    // The toggle has focus, and Space flips it back.
    pair.key(' ');
    pair.frame();
    CHECK_FALSE(values[2]);
    CHECK(pair.scripted.number("heard(physics, 'change')") == 2);
    CHECK(pair.scripted.number("last(physics, 'change').value") == 0);

    pair.click(pair.direct.center(rows[1]));
    pair.frame();
    pair.click(pair.direct.center(rows[3]));
    pair.frame(30);
    CHECK(pair.scripted.number("heard(locked, 'change') + heard(snap, 'change')") == 0);
    CHECK(pair.alike());

    // The value that the script sets replaces the one that the click left.
    pair.scripted.run("kitTree.set(shadows, 'value', ''); kitTree.set(physics, 'value', 'true');");
    values[0] = true;
    values[2] = true;
    pair.frame(30);
    CHECK(pair.scripted.number("log.length") == 3);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.set(shadows, 'value', null); kitTree.set(physics, 'value', 'false');");
    values[0] = false;
    values[2] = false;
    pair.frame(30);
    CHECK(pair.alike());
}

TEST_CASE("a tab strip shows its tab nodes, selects the tab that the script or the user chooses, and reports a close") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(kTabs);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {padding: '16'});
        var strip = make('tab-strip', page, {size: 'sm', selected: '2', 'max-width': '400'});
        tab(strip, 1, 'Console');
        tab(strip, 2, 'Output');
        tab(strip, 3, 'Profiler');
        make('badge', tabs[3], {variant: 'warning'}, '3');
        make('tab-close', tabs[3], {});
        var body = make('label', page, {}, 'tab 2');
        hear(strip, 'select', function (detail) { kitTree.setText(body, 'tab ' + detail.value); });
        hear(tabs[3], 'remove', function (detail) { kitTree.destroy(tabs[detail.value]); });
    )JS");
    TestTabs held(3);
    prKitTabsSelect(held.tabs, 2);
    PrLayoutNode nodes[5] = {};
    PrLayoutNode close = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.padding = prKitEdgesAll(16)});
        PrLayoutNode strip = prKitTabStrip(kit, page, {.tabs = held.tabs}, {.size = PrKitControlSize_Sm});
        prKitPlace(kit, strip, {.maxWidth = prKitPx(400)});
        declareTabs(kit, strip, held.tabs, nodes);
        bool closed = false;
        if (prKitTabsIndex(held.tabs, 3) < prKitTabsCount(held.tabs)) {
            prKitBadge(kit, nodes[3], {.text = "3"}, {.variant = PrKitBadgeVariant_Warning});
            close = prKitTabClose(kit, nodes[3]);
            closed = prKitActivated(kit, close);
        }
        const char* const bodies[] = {"tab 0", "tab 1", "tab 2", "tab 3", "tab 4"};
        prKitLabel(kit, page, {.text = bodies[prKitTabsSelected(held.tabs)]}, {});
        if (closed)
            prKitTabsRemove(held.tabs, 3);
    };
    pair.frame();
    CHECK(pair.alike());
    // The script chose the selected tab, so the strip tells it of none.
    CHECK(pair.scripted.number("log.length") == 0);

    // The strip tells the script of a click's selection in the round that routes the click.
    pair.click(pair.direct.center(nodes[3]));
    pair.clickRounds();
    CHECK(prKitTabsSelected(held.tabs) == 3);
    CHECK(pair.scripted.number("heard(strip, 'select')") == 1);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 3);
    pair.frame(30);
    CHECK(pair.alike());
    pair.key(PrAppKey_Left);
    pair.frame();
    CHECK(prKitTabsSelected(held.tabs) == 2);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 2);
    pair.frame(30);
    CHECK(pair.alike());

    // The script selects a tab, and hears of no selection for it. A tab that the strip does not hold is not selected,
    // and the strip says which tab is.
    pair.scripted.run("kitTree.set(strip, 'selected', '3'); kitTree.setText(body, 'tab 3');");
    prKitTabsSelect(held.tabs, 3);
    pair.frame(30);
    CHECK(pair.scripted.number("heard(strip, 'select')") == 2);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.set(strip, 'selected', '9');");
    pair.frame();
    CHECK(pair.scripted.number("heard(strip, 'select')") == 3);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 3);

    // The script steps the selection around the strip's ends, and the strip says which tab each step selected.
    pair.scripted.run("kitTree.set(strip, 'selected', 'next');");
    CHECK(prKitTabsStep(held.tabs, 1) == 1);
    pair.frame();
    CHECK(pair.scripted.number("heard(strip, 'select')") == 4);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 1);
    pair.frame(30);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.set(strip, 'selected', 'previous');");
    CHECK(prKitTabsStep(held.tabs, -1) == 3);
    pair.frame(2);
    CHECK(pair.scripted.number("heard(strip, 'select')") == 5);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 3);
    pair.frame(30);
    CHECK(pair.alike());

    // The close control of the selected tab asks the script to remove it, and the strip selects the tab before it once
    // the script does.
    pair.click(pair.direct.center(close));
    pair.frame();
    CHECK(pair.scripted.number("heard(tabs[3], 'remove')") == 1);
    CHECK(pair.scripted.number("last(tabs[3], 'remove').value") == 3);
    CHECK(prKitTabsCount(held.tabs) == 2);
    CHECK(prKitTabsSelected(held.tabs) == 2);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 2);
    pair.frame(30);
    CHECK(pair.alike());

    // A tab that the script adds goes after the tab that its node follows.
    pair.scripted.run("kitTree.insert(strip, tab(strip, 4, 'Animator'), tabs[2]);");
    prKitTabsInsert(held.tabs, 4, 1);
    pair.frame(30);
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard(strip, 'select')") == 6);
}

TEST_CASE("a reorderable strip moves the tab that the user drags, tells the script, and takes the order that the script gives its nodes") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(kTabs);
    pair.scripted.run(R"JS(
        var strip = make('tab-strip', 0, {reorderable: '', selected: '2'});
        tab(strip, 1, 'Console');
        tab(strip, 2, 'Output');
        tab(strip, 3, 'Profiler');
        hear(strip, 'select');
        hear(strip, 'drop');
    )JS");
    TestTabs held(3);
    prKitTabsSelect(held.tabs, 2);
    PrLayoutNode nodes[5] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrLayoutNode strip = prKitTabStrip(pair.direct.kit, root, {.tabs = held.tabs, .reorderable = true}, {});
        declareTabs(pair.direct.kit, strip, held.tabs, nodes);
    };
    pair.frame();
    CHECK(pair.alike());

    // The first tab is dragged past the last, where the kit puts it and selects it.
    PrVec2 start = pair.direct.center(nodes[1]);
    PrVec2 end{pair.direct.bounds(nodes[3]).right - 2, start.y};
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = start}});
    pair.frame();
    pair.move(PrVec2{start.x + 10, start.y});
    pair.frame();
    pair.move(end);
    pair.frame();
    // The caret that marks where the tab will go shows in both.
    CHECK(pair.alike());
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = end}});
    pair.frame(2);
    REQUIRE(prKitTabsAt(held.tabs, 2) == 1);
    CHECK(pair.scripted.number("heard(strip, 'drop')") == 1);
    CHECK(pair.scripted.number("last(strip, 'drop').values[0]") == 1);
    CHECK(pair.scripted.number("last(strip, 'drop').values[1]") == 2);
    CHECK(pair.scripted.number("heard(strip, 'select')") == 1);
    CHECK(pair.scripted.number("last(strip, 'select').value") == 1);
    pair.frame(30);
    CHECK(pair.alike());

    // The strip keeps the order that the drag left while the script adds a tab, which goes after the tab that its node
    // follows, and removes it.
    pair.scripted.run("kitTree.insert(strip, tab(strip, 4, 'Animator'), tabs[2]);");
    prKitTabsInsert(held.tabs, 4, 3);
    pair.frame(30);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.destroy(tabs[4]);");
    prKitTabsRemove(held.tabs, 4);
    pair.frame(30);
    CHECK(pair.alike());

    // The script moves the tab's node to where the tab went, which changes nothing.
    pair.scripted.run("kitTree.insert(strip, tabs[1], 0);");
    pair.frame(30);
    CHECK(pair.alike());

    // The script puts the last node first, and the strip takes the nodes' order and keeps its selection.
    pair.scripted.run("kitTree.insert(strip, tabs[1], tabs[2]);");
    prKitTabsMove(held.tabs, held.tabs, 1, 0);
    pair.frame(30);
    CHECK(pair.alike());
    pair.scripted.run("kitTree.insert(strip, tabs[3], tabs[1]);");
    prKitTabsMove(held.tabs, held.tabs, 3, 0);
    prKitTabsSelect(held.tabs, 1);
    pair.frame(30);
    CHECK(pair.alike());
    CHECK(pair.scripted.number("log.length") == 2);
}

TEST_CASE("a strip of a group reports a tab of another strip that is dropped on it, selects it once the script moves it, and reports a tab torn off") {
    DragStartWindow windows[2];
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(kTabs);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {gap: '24', padding: '16'});
        var upper = make('tab-strip', page, {group: '77', reorderable: ''});
        tab(upper, 1, 'Console');
        tab(upper, 2, 'Output');
        tab(upper, 3, 'Profiler');
        var lower = make('tab-strip', page, {group: '77', reorderable: ''});
        var order = [10, 11];
        tab(lower, 10, 'Scene');
        tab(lower, 11, 'Game');
        hear(lower, 'select');
        hear(lower, 'drop', function (detail) {
            const id = detail.values[0];
            const index = detail.values[1];
            kitTree.insert(lower, tabs[id], index < order.length ? tabs[order[index]] : 0);
            order.splice(index, 0, id);
        });
        hear(upper, 'select');
        hear(upper, 'tearoff', function (detail) { kitTree.destroy(tabs[detail.values[0]]); });
    )JS");
    TestTabs upper(3);
    TestTabs lower(2, 10);
    PrLayoutNode nodes[12] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.gap = 24, .padding = prKitEdgesAll(16)});
        PrKitTabMove moves[2] = {};
        PrLayoutNode first = prKitTabStrip(kit, page, {.tabs = upper.tabs, .group = 77, .reorderable = true}, {});
        bool tornOff = prKitTabMoved(kit, first, &moves[0]);
        declareTabs(kit, first, upper.tabs, nodes);
        PrLayoutNode second = prKitTabStrip(kit, page, {.tabs = lower.tabs, .group = 77, .reorderable = true}, {});
        bool received = prKitTabMoved(kit, second, &moves[1]);
        declareTabs(kit, second, lower.tabs, nodes);
        // The application carries a move out once the round has created the tab where it was.
        if (received)
            prKitTabsMove(upper.tabs, lower.tabs, moves[1].id, moves[1].index);
        if (tornOff)
            prKitTabsRemove(upper.tabs, moves[0].id);
    };
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("last(upper, 'select').value") == 1);
    CHECK(pair.scripted.number("last(lower, 'select').value") == 10);

    // The second tab of the upper strip is dropped on the right half of the lower strip's first tab.
    PrVec2 start = pair.direct.center(nodes[2]);
    PrRect target = pair.direct.bounds(nodes[10]);
    PrVec2 end{target.right - 2, (target.top + target.bottom) / 2};
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = start}});
    pair.frame();
    pair.move(PrVec2{start.x + 10, start.y});
    pair.frame();
    pair.move(end);
    pair.frame();
    CHECK(pair.alike());
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = end}});
    pair.frame(2);
    REQUIRE(prKitTabsCount(lower.tabs) == 3);
    CHECK(prKitTabsAt(lower.tabs, 1) == 2);
    CHECK(pair.scripted.number("heard(lower, 'drop')") == 1);
    CHECK(pair.scripted.number("last(lower, 'drop').values[0]") == 2);
    CHECK(pair.scripted.number("last(lower, 'drop').values[1]") == 1);
    CHECK(pair.scripted.number("last(lower, 'select').value") == 2);
    CHECK(pair.scripted.number("heard(upper, 'select')") == 1);
    pair.frame(30);
    CHECK(pair.alike());

    // A tab that is dragged out of the window goes to the system, and tears off where the system's drag ends outside
    // every window. A window that reports no pointer gives the origin of no display.
    prUiSetWindow(pair.scripted.fixture.ui, &windows[0]);
    prUiSetWindow(pair.direct.ui, &windows[1]);
    start = pair.direct.center(nodes[1]);
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = start}});
    pair.frame();
    pair.move(PrVec2{start.x + 10, start.y});
    pair.frame();
    pair.move(PrVec2{start.x, -20});
    pair.frame();
    REQUIRE(windows[0].starts == 1);
    REQUIRE(windows[1].starts == 1);
    pair.push(PrAppEvent{.kind = PrAppEventKind_DragEnd, .drag = {.operation = PrAppDragOperation_None}});
    pair.frame(2);
    CHECK(prKitTabsCount(upper.tabs) == 1);
    CHECK(pair.scripted.number("heard(upper, 'tearoff')") == 1);
    CHECK(pair.scripted.number("last(upper, 'tearoff').values.length") == 3);
    CHECK(pair.scripted.number("last(upper, 'tearoff').values[0]") == 1);
    CHECK(pair.scripted.number("last(upper, 'tearoff').values[1] + last(upper, 'tearoff').values[2]") == 0);
    CHECK(pair.scripted.number("last(upper, 'tearoff').text === '0' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("last(upper, 'select').value") == 3);
    pair.frame(30);
    CHECK(pair.alike());
    prUiSetWindow(pair.scripted.fixture.ui, nullptr);
    prUiSetWindow(pair.direct.ui, nullptr);
}

TEST_CASE("a strip that collapses while single takes no room, and a disabled tab or strip takes no click") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(kTabs);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {padding: '16'});
        var single = make('tab-strip', page, {'collapses-single': ''});
        tab(single, 1, 'Console');
        make('label', page, {}, 'below the strip');
        var partly = hear(make('tab-strip', page, {}), 'select');
        tab(partly, 3, 'Profiler');
        kitTree.set(tab(partly, 4, 'Animator'), 'disabled', '');
        var disabled = hear(make('tab-strip', page, {disabled: ''}), 'select');
        tab(disabled, 5, 'Scene');
        tab(disabled, 6, 'Game');
    )JS");
    TestTabs single(1);
    TestTabs partly(2, 3);
    TestTabs disabled(2, 5);
    PrLayoutNode nodes[7] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.padding = prKitEdgesAll(16)});
        PrLayoutNode strip = prKitTabStrip(kit, page, {.tabs = single.tabs, .collapsesSingle = true}, {});
        declareTabs(kit, strip, single.tabs, nodes);
        prKitLabel(kit, page, {.text = "below the strip"}, {});
        strip = prKitTabStrip(kit, page, {.tabs = partly.tabs}, {});
        nodes[3] = prKitTab(kit, strip, {.id = 3});
        prKitTabLabel(kit, nodes[3], {.text = tabName(3)});
        nodes[4] = prKitTab(kit, strip, {.id = 4, .disabled = true});
        prKitTabLabel(kit, nodes[4], {.text = tabName(4)});
        strip = prKitTabStrip(kit, page, {.tabs = disabled.tabs, .disabled = true}, {});
        declareTabs(kit, strip, disabled.tabs, nodes);
    };
    pair.frame();
    CHECK(pair.alike());
    // Each strip chose its first tab, and says so.
    CHECK(pair.scripted.number("last(partly, 'select').value") == 3);
    CHECK(pair.scripted.number("last(disabled, 'select').value") == 5);
    pair.click(pair.direct.center(nodes[4]));
    pair.frame();
    pair.click(pair.direct.center(nodes[6]));
    pair.frame(30);
    CHECK(prKitTabsSelected(partly.tabs) == 3);
    CHECK(prKitTabsSelected(disabled.tabs) == 5);
    CHECK(pair.scripted.number("log.length") == 2);
    CHECK(pair.alike());

    // The strip shows once it holds a second tab.
    pair.scripted.run("tab(single, 2, 'Output');");
    prKitTabsInsert(single.tabs, 2, 1);
    pair.frame(30);
    CHECK(pair.alike());
}

TEST_CASE("progress bars, meters and spinners render as the kit's own in every frame") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {align: 'start', gap: '12', padding: '16'});
        var bake = make('progress', page, {value: '0.4', width: '200'});
        make('progress', page, {indeterminate: '', width: '200'});
        var memory = make('meter', page, {value: '0.6', width: '200'});
        make('meter', page, {value: '15', low: '10', high: '20', tone: 'success', width: '200'});
        make('spinner', page, {});
        var large = make('spinner', page, {size: '32'});
    )JS");
    float bake = 0.4f;
    float memory = 0.6f;
    float large = 32;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 12, .padding = prKitEdgesAll(16)});
        PrKitPlace wide{.width = prKitPx(200)};
        prKitPlace(kit, prKitProgress(kit, page, {.value = bake}), wide);
        prKitPlace(kit, prKitProgress(kit, page, {.indeterminate = true}), wide);
        prKitPlace(kit, prKitMeter(kit, page, {.value = memory}, {}), wide);
        prKitPlace(kit, prKitMeter(kit, page, {.value = 15, .low = 10, .high = 20}, {.tone = PrKitTone_Success}), wide);
        prKitSpinner(kit, page, {}, {});
        prKitSpinner(kit, page, {}, {.size = large});
    };
    // The band of the indeterminate bar and the spinners move with the frames' time, which both share.
    for (uint32_t i = 0; i < 8; ++i) {
        pair.frame(1, 0.13);
        CHECK(pair.alike());
    }
    pair.scripted.run(R"JS(
        kitTree.set(bake, 'value', '0.9');
        kitTree.set(memory, 'value', '0.25');
        kitTree.set(large, 'size', null);
    )JS");
    bake = 0.9f;
    memory = 0.25f;
    large = 0;
    pair.frame(3, 0.13);
    CHECK(pair.alike());
}

TEST_CASE("the choices take only the values of their attributes, and a refused value changes nothing") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {align: 'start', gap: '12', padding: '16'});
        var track = make('segmented', page, {size: 'sm'});
        var segment = make('segment', track, {icon: 'cube', selected: ''}, 'Local');
        var group = make('radio-group', page, {direction: 'row'});
        var radio = make('radio', group, {}, 'Unlit');
        var checkbox = make('checkbox', page, {value: ''}, 'Locked');
        var toggle = make('toggle', page, {}, 'Snap');
        var strip = make('tab-strip', page, {selected: '2'});
        var tabs = [1, 2].map(function (id) { return make('tab', strip, {id: String(id)}); });
        var label = make('tab-label', tabs[0], {}, 'Console');
        var close = make('tab-close', tabs[1], {});
        var progress = make('progress', page, {value: '0.5', width: '200'});
        var meter = make('meter', page, {value: '0.5', tone: 'warning', width: '200'});
        var spinner = make('spinner', page, {size: '24'});
        var refused = [
            kitTree.set(track, 'size', 'huge'), kitTree.set(track, 'direction', 'row'), kitTree.set(track, 'disabled', 'no'),
            kitTree.set(segment, 'icon', 'no-such-icon'), kitTree.set(segment, 'selected', '1'),
            kitTree.set(segment, 'value', ''), kitTree.set(group, 'direction', 'diagonal'),
            kitTree.set(radio, 'disabled', 'yes'), kitTree.set(checkbox, 'value', 'maybe'),
            kitTree.set(checkbox, 'checked', ''), kitTree.set(toggle, 'size', '26'),
            kitTree.set(strip, 'selected', 'second'), kitTree.set(strip, 'selected', '-1'),
            kitTree.set(strip, 'selected', 'last'),
            kitTree.set(strip, 'group', '1.5'), kitTree.set(strip, 'direction', 'row'),
            kitTree.set(strip, 'reorderable', 'on'), kitTree.set(tabs[0], 'id', 'one'), kitTree.set(tabs[0], 'id', '-3'),
            kitTree.set(tabs[0], 'selected', ''), kitTree.set(label, 'role', 'title'), kitTree.set(close, 'disabled', ''),
            kitTree.set(progress, 'value', 'half'), kitTree.set(progress, 'indeterminate', '2'),
            kitTree.set(meter, 'tone', 'loud'), kitTree.set(meter, 'high', ''), kitTree.set(spinner, 'size', '-4'),
            kitTree.set(spinner, 'size', 'sm'),
            kitTree.listen(track, 'click', true), kitTree.listen(segment, 'change', true),
            kitTree.listen(checkbox, 'click', true), kitTree.listen(strip, 'remove', true),
            kitTree.listen(tabs[0], 'select', true), kitTree.listen(close, 'click', true),
            kitTree.listen(progress, 'change', true),
        ].filter(Boolean).length;
        var taken = [
            kitTree.listen(segment, 'click', true), kitTree.listen(radio, 'click', true),
            kitTree.listen(checkbox, 'change', true), kitTree.listen(toggle, 'change', true),
            kitTree.listen(strip, 'select', true), kitTree.listen(strip, 'drop', true),
            kitTree.listen(strip, 'tearoff', true), kitTree.listen(tabs[0], 'remove', true),
            kitTree.set(strip, 'group', '7'), kitTree.set(strip, 'group', null), kitTree.set(meter, 'low', '-1'),
            kitTree.set(meter, 'low', null),
        ].filter(Boolean).length;
    )JS");
    CHECK(pair.scripted.number("refused") == 0);
    CHECK(pair.scripted.number("taken") == 12);
    TestTabs held(2);
    prKitTabsSelect(held.tabs, 2);
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 12, .padding = prKitEdgesAll(16)});
        PrLayoutNode track = prKitSegmented(kit, page, {}, {.size = PrKitControlSize_Sm});
        prKitSegment(kit, track, {.label = "Local", .icon = PrKitIconKind_Cube, .selected = true});
        PrLayoutNode group = prKitRadioGroup(kit, page, {}, {.direction = PrKitDirection_Row});
        prKitRadio(kit, group, {.label = "Unlit"});
        bool locked = true;
        bool snap = false;
        prKitCheckbox(kit, page, {.value = &locked, .label = "Locked"}, {});
        prKitToggle(kit, page, {.value = &snap, .label = "Snap"}, {});
        PrLayoutNode strip = prKitTabStrip(kit, page, {.tabs = held.tabs}, {});
        prKitTabLabel(kit, prKitTab(kit, strip, {.id = 1}), {.text = "Console"});
        prKitTabClose(kit, prKitTab(kit, strip, {.id = 2}));
        PrKitPlace wide{.width = prKitPx(200)};
        prKitPlace(kit, prKitProgress(kit, page, {.value = 0.5f}), wide);
        prKitPlace(kit, prKitMeter(kit, page, {.value = 0.5f}, {.tone = PrKitTone_Warning}), wide);
        prKitSpinner(kit, page, {}, {.size = 24});
    };
    pair.frame(2, 0.25);
    CHECK(pair.alike());
}

TEST_CASE("a part of a choice that is not under its container, and a tab with no id of its own, are not declared") {
    Pair pair;
    pair.scripted.run(kJsKitMake);
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var page = make('box', 0, {gap: '12', padding: '16'});
        make('segment', page, {}, 'a segment under a box');
        make('radio', page, {}, 'a radio under a box');
        make('tab', page, {id: '1'});
        make('tab-label', page, {}, 'a label under a box');
        make('tab-close', page, {});
        var track = make('segmented', page, {});
        make('radio', track, {}, 'a radio under a segmented control');
        make('segment', track, {}, 'Solid');
        make('tab', track, {id: '2'});
        var group = make('radio-group', page, {});
        make('segment', group, {}, 'a segment under a radio group');
        make('radio', group, {}, 'Lambert');
        var strip = hear(make('tab-strip', page, {}), 'select');
        make('tab-label', strip, {}, 'a label under a strip');
        var unnamed = make('tab', strip, {});
        make('tab-label', unnamed, {}, 'a tab with no id');
        var first = make('tab', strip, {id: '5'});
        make('tab-label', first, {}, 'Console');
        var again = make('tab', strip, {id: '5'});
        make('tab-label', again, {}, 'a tab with the id of another');
        make('tab-close', again, {});
        make('label', page, {}, 'the end');
    )JS");
    TestTabs held(0);
    prKitTabsInsert(held.tabs, 5, 0);
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.gap = 12, .padding = prKitEdgesAll(16)});
        prKitSegment(kit, prKitSegmented(kit, page, {}, {}), {.label = "Solid"});
        prKitRadio(kit, prKitRadioGroup(kit, page, {}, {}), {.label = "Lambert"});
        PrLayoutNode strip = prKitTabStrip(kit, page, {.tabs = held.tabs}, {});
        prKitTabLabel(kit, prKitTab(kit, strip, {.id = 5}), {.text = "Console"});
        prKitLabel(kit, page, {.text = "the end"}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.number("last(strip, 'select').value") == 5);

    // The tab that had the id of another is declared once it is the only one with it, and takes its place in the strip.
    pair.scripted.run("kitTree.destroy(first); kitTree.set(unnamed, 'id', '6');");
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.gap = 12, .padding = prKitEdgesAll(16)});
        prKitSegment(kit, prKitSegmented(kit, page, {}, {}), {.label = "Solid"});
        prKitRadio(kit, prKitRadioGroup(kit, page, {}, {}), {.label = "Lambert"});
        PrLayoutNode strip = prKitTabStrip(kit, page, {.tabs = held.tabs}, {});
        prKitTabLabel(kit, prKitTab(kit, strip, {.id = 6}), {.text = "a tab with no id"});
        PrLayoutNode again = prKitTab(kit, strip, {.id = 5});
        prKitTabLabel(kit, again, {.text = "a tab with the id of another"});
        prKitTabClose(kit, again);
        prKitLabel(kit, page, {.text = "the end"}, {});
    };
    prKitTabsInsert(held.tabs, 6, 0);
    pair.frame(2);
    CHECK(pair.alike());
}
