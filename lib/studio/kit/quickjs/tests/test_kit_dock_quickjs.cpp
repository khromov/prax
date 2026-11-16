// The kinds of Studio Kit's docks and splits in its QuickJS binding: each declares what the same calls to the kit
// declare.

#include "pr/array.h"
#include "pr/kit_box.h"
#include "pr/kit_dock.h"
#include "pr/kit_text.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// The script's record of the events that it hears: each with its node, its type and its detail, in `heard`. `count` and
// `last` tell how many events of a type a node queued, and the detail of the last of them. A test's `answer`, if it
// defines one, hears each event too.
const char* const kHear = R"JS(
    var heard = [];
    kitTree.onevent = function (node, type, detail) {
        heard.push({node: node, type: type, detail: detail});
        if (typeof answer === 'function')
            answer(node, type, detail);
    };
    function count(node, type) {
        return heard.filter(function (event) { return event.node === node && event.type === type; }).length;
    }
    function last(node, type) {
        for (var i = heard.length - 1; i >= 0; --i) {
            if (heard[i].node === node && heard[i].type === type)
                return heard[i].detail;
        }
        return null;
    }
)JS";

// Runs `frames` frames of both kits, which is enough for a pointer's hover and a press to settle in each.
void settle(KitFixture& first, KitFixture& second, uint32_t frames = 30) {
    for (uint32_t i = 0; i < frames; ++i) {
        first.frame();
        second.frame();
    }
}

// Presses the primary button at `from`, moves to `to` over frames, and releases there, as a user drags.
void drag(KitFixture& f, PrVec2 from, PrVec2 to) {
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = from}});
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
    f.frame();
    f.move(PrVec2{from.x + (to.x - from.x) / 8, from.y + (to.y - from.y) / 8});
    f.frame();
    f.move(to);
    f.frame();
    f.frame();
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = to}});
    f.frame();
}

// ---------------------------------------------------------------------------------------------------------------------
// Splits

// A split of two panes along a row, 200 px tall, that the kit's own calls declare, with a label in each pane.
struct DirectSplit {
    KitFixture fixture;
    float weights[3] = {1, 3, 1};
    uint32_t count = 2;
    PrKitSplitStyle style{
        .direction = PrKitDirection_Row,
        .minShare = 0.2f,
        .place = {.height = prKitPx(200), .flex = PrKitFlex_Fixed},
    };
    PrLayoutNode panes[3] = {};

    DirectSplit() {
        fixture.build = [this](PrLayoutNode root) {
            static const char* const kNames[] = {"First", "Second", "Third"};
            PrLayoutNode split = prKitSplit(fixture.kit, root, {.count = count, .weights = weights}, style);
            for (uint32_t i = 0; i < count; ++i) {
                panes[i] = prKitSplitPane(fixture.kit, split, i);
                prKitLabel(fixture.kit, panes[i], {.text = kNames[i]}, {});
            }
        };
    }

    // Returns the middle of the band after pane `index`.
    PrVec2 band(uint32_t index) {
        PrRect pane = fixture.bounds(panes[index]);
        if (style.direction == PrKitDirection_Row)
            return PrVec2{pane.right + kPrKitSplitBand / 2, (pane.top + pane.bottom) / 2};
        return PrVec2{(pane.left + pane.right) / 2, pane.bottom + kPrKitSplitBand / 2};
    }
};

// The script that makes the split of DirectSplit, and listens for its `change`.
const char* const kSplitScript = R"JS(
    var split = make('split', 0, {direction: 'row', 'min-share': '0.2', weights: '1 3', height: '200', flex: 'fixed'});
    var panes = ['First', 'Second'].map(function (name) {
        const pane = make('split-pane', split, {});
        make('label', pane, {}, name);
        return pane;
    });
    kitTree.listen(split, 'change', true);
)JS";

// ---------------------------------------------------------------------------------------------------------------------
// Docks

// The panels of the tests' docks, which the scripts name in lower case.
enum : PrKitDockPanelId {
    Panel_Scene = 1,
    Panel_Viewport,
    Panel_Inspector,
    Panel_Project,
    Panel_Console,
    Panel_Notes,
};
typedef PrKitDockPanelId Panel;

const char* const kTitles[] = {"", "Scene", "Viewport", "Inspector", "Project", "Console", "Notes"};

// What the tab of each panel shows, by panel, which a test changes as its script changes a pane.
struct Tabs {
    const char* titles[7] = {"", "Scene", "Viewport", "Inspector", "Project", "Console", "Notes"};
    bool dirty[7] = {false, false, false, false, false, true, false};
};

PrKitDockTab describe(void* context, PrKitDockPanelId panel) {
    const Tabs& tabs = *static_cast<const Tabs*>(context);
    return PrKitDockTab{.title = tabs.titles[panel], .dirty = tabs.dirty[panel]};
}

// What the tab of any panel of a dock that no script made shows.
PrKitDockTab describeNative(void*, PrKitDockPanelId) {
    return PrKitDockTab{.title = "Native"};
}

// What a dock of Scene alone shows for a tab: a title for its own panel, and nothing for another dock's.
PrKitDockTab describeScene(void*, PrKitDockPanelId panel) {
    return panel == Panel_Scene ? PrKitDockTab{.title = "Scene"} : PrKitDockTab{};
}

// A dock that the kit's own calls declare, with a label in each open panel, as the tests' scripts make theirs.
struct DirectDock {
    KitFixture fixture;
    PrKitDock* dock = prKitDockCreate();
    PrLayoutNode area = kPrLayoutNodeNone;
    Array<PrKitDockPane> panes;
    Tabs tabs;
    bool placed = false;

    DirectDock() {
        fixture.build = [this](PrLayoutNode root) {
            area = prKitDockBuild(fixture.kit, root, {.dock = dock, .describe = describe, .context = &tabs});
            if (placed)
                prKitPlace(fixture.kit, area, {.margin = prKitEdgesAll(12), .flex = PrKitFlex_Fill});
            panes.clear();
            for (uint32_t i = 0; i < prKitDockPaneCount(dock); ++i) {
                PrKitDockPane pane = prKitDockPane(dock, i);
                panes.push(pane);
                prKitLabel(fixture.kit, pane.content, {.text = kTitles[pane.panel]}, {});
            }
        };
    }
    ~DirectDock() { prKitDockDestroy(dock); }

    // Seeds an editor's layout: Scene over Project, Viewport over Console, and Inspector, in three columns of a
    // quarter, a half and a quarter. Scene's leaf is the primary leaf.
    void seedEditor() {
        PrKitDockNode scene = prKitDockPrimary(dock);
        prKitDockAddPanel(dock, scene, Panel_Scene);
        PrKitDockNode viewport = prKitDockSplit(dock, scene, PrKitDockEdge_Right, Panel_Viewport, 0.5f);
        prKitDockSplit(dock, viewport, PrKitDockEdge_Right, Panel_Inspector, 0.5f);
        prKitDockSplit(dock, scene, PrKitDockEdge_Bottom, Panel_Project, 0.5f);
        prKitDockSplit(dock, viewport, PrKitDockEdge_Bottom, Panel_Console, 0.5f);
        const float columns[] = {0.25f, 0.5f, 0.25f};
        prKitDockSplitSetWeights(dock, prKitDockRoot(dock), columns, 3);
    }

    // Returns the box of the content of `panel`, which is open.
    PrRect content(PrKitDockPanelId panel) {
        for (const PrKitDockPane& pane : panes) {
            if (pane.panel == panel)
                return fixture.bounds(pane.content);
        }
        return PrRect{};
    }

    // Returns a point on the tab of `panel`, which is the first tab of its leaf and open.
    PrVec2 tab(PrKitDockPanelId panel) {
        PrRect box = content(panel);
        return PrVec2{box.left + 10, box.top - 11};
    }

    // Returns the middle of the close button on the tab of `panel`, which is the first tab of its leaf and open. The
    // button is the tab's square press target, found by hit testing along the strip.
    PrVec2 closeButton(PrKitDockPanelId panel) {
        PrRect box = content(panel);
        Array<PrLayoutNode> hits(32);
        for (float x = box.left + 20; x < box.right; x += 1) {
            PrLayoutVec at{prLayoutUnitFromPx(x), prLayoutUnitFromPx(box.top - 11)};
            uint32_t count = prLayoutTreeHitTest(fixture.tree, at, hits.data(), hits.count());
            for (uint32_t i = 0; i < count && i < hits.count(); ++i) {
                PrRect hit = fixture.bounds(hits[i]);
                if ((prUiFlags(fixture.ui, hits[i]) & PrUiFlag_Focusable) != 0 &&
                        hit.right - hit.left == kPrKitHeightXs)
                    return fixture.center(hits[i]);
            }
        }
        return PrVec2{};
    }
};

// The layout of DirectDock::seedEditor as a script gives it.
const char* const kEditorLayout = R"JS(
    var layout = {direction: 'row', children: [
        {weight: 1, direction: 'column', children: [{tabs: ['scene'], primary: true}, {tabs: ['project']}]},
        {weight: 2, direction: 'column', children: [{tabs: ['viewport']}, {tabs: ['console']}]},
        {weight: 1, tabs: ['inspector']},
    ]};
)JS";

// The script that makes a dock of `layout` with the tests' six panes, each of which holds a label of its title, and
// listens for the dock's `change` and each pane's `remove`. `shown` reads the layout that the last `change` told.
const char* const kDockScript = R"JS(
    var dock = make('dock', 0, {layout: JSON.stringify(layout)});
    var panes = {};
    ['Scene', 'Viewport', 'Inspector', 'Project', 'Console', 'Notes'].forEach(function (title) {
        const pane = make('dock-pane', dock, {name: title.toLowerCase()}, title);
        make('label', pane, {}, title);
        kitTree.listen(pane, 'remove', true);
        panes[title.toLowerCase()] = pane;
    });
    kitTree.set(panes.console, 'dirty', '');
    kitTree.listen(dock, 'change', true);
    function shown() {
        return JSON.parse(last(dock, 'change').text);
    }
)JS";

// A script's dock of the editor's layout, and the same dock of the kit's own calls.
struct DockPair {
    JsKitScripted scripted;
    DirectDock direct;

    explicit DockPair(const char* layout = kEditorLayout) {
        scripted.run(kJsKitMake);
        scripted.run(kHear);
        scripted.run(layout);
        scripted.run(kDockScript);
        if (layout == kEditorLayout)
            direct.seedEditor();
    }

    void frame() {
        scripted.fixture.frame();
        direct.fixture.frame();
    }

    // Returns true if both render the same pixels, and the script's tree is laid out as a layout from scratch lays it
    // out.
    bool alike() {
        return jsKitRendersAlike(scripted.fixture, direct.fixture) && kitVerified(scripted.fixture.tree);
    }
};

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a split of a script divides its box by its weights, and a band's drag reaches the script as the weights") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run(kSplitScript);
    DirectSplit direct;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(kitVerified(scripted.fixture.tree));
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));

    // The band between the panes, dragged 100 px toward the second pane, moves weight to the first.
    PrVec2 band = direct.band(0);
    drag(scripted.fixture, band, PrVec2{band.x + 100, band.y});
    drag(direct.fixture, band, PrVec2{band.x + 100, band.y});
    CHECK(direct.weights[0] > 1.4f);
    CHECK(scripted.number("count(split, 'change')") >= 1);
    CHECK(scripted.number("last(split, 'change').values.length") == 3);
    CHECK(scripted.number("last(split, 'change').values[0]") == 0);
    CHECK(scripted.number("last(split, 'change').values[1]") == double(direct.weights[0]));
    CHECK(scripted.number("last(split, 'change').values[2]") == double(direct.weights[1]));
    CHECK(scripted.number("last(split, 'change').text.split(' ').length") == 2);
    CHECK(float(scripted.number("Number(last(split, 'change').text.split(' ')[0])")) == direct.weights[0]);
    CHECK(float(scripted.number("Number(last(split, 'change').text.split(' ')[1])")) == direct.weights[1]);
    settle(scripted.fixture, direct.fixture);
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    CHECK(kitVerified(scripted.fixture.tree));

    // The drag's weights stand until the script sets others. Setting what the event told changes nothing.
    scripted.run("kitTree.set(split, 'weights', last(split, 'change').text);");
    settle(scripted.fixture, direct.fixture, 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    scripted.run("kitTree.set(split, 'weights', '2 1');");
    direct.weights[0] = 2;
    direct.weights[1] = 1;
    settle(scripted.fixture, direct.fixture, 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
}

TEST_CASE("a band of a split leaves each pane the split's smallest share, down a column as along a row") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run(kSplitScript);
    scripted.run(R"JS(
        kitTree.set(split, 'direction', null);
        kitTree.set(split, 'weights', '1 3 1');
        panes.push(make('split-pane', split, {}));
        make('label', panes[2], {}, 'Third');
    )JS");
    DirectSplit direct;
    direct.style.direction = PrKitDirection_Column;
    direct.count = 3;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));

    // The second band, dragged up past the first, leaves the second pane a fifth of the total.
    PrVec2 band = direct.band(1);
    drag(scripted.fixture, band, PrVec2{band.x, 0});
    drag(direct.fixture, band, PrVec2{band.x, 0});
    CHECK(direct.weights[1] == doctest::Approx(1.0));
    CHECK(direct.weights[2] == doctest::Approx(3.0));
    CHECK(scripted.number("last(split, 'change').values[0]") == 1);
    CHECK(scripted.number("last(split, 'change').values[1]") == double(direct.weights[1]));
    CHECK(scripted.number("last(split, 'change').values[2]") == double(direct.weights[2]));
    CHECK(scripted.number("last(split, 'change').text.split(' ').length") == 3);
    settle(scripted.fixture, direct.fixture);
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a split with no weights gives each pane one, and a split with no panes is its box") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var split = make('split', 0, {direction: 'row', weights: '5', height: '120', flex: 'fixed'});
        var first = make('split-pane', split, {});
        make('label', first, {}, 'First');
        make('label', split, {}, 'A label that is no pane, which a split does not declare');
        var second = make('split-pane', split, {});
        make('label', second, {}, 'Second');
        var empty = make('split', 0, {height: '40', flex: 'fixed'});
        var stray = make('split-pane', 0, {});
        make('label', stray, {}, 'A pane under no split, which declares nothing');
        make('label', 0, {}, 'After');
    )JS");
    KitFixture direct;
    float weights[2] = {5, 1};
    direct.build = [&](PrLayoutNode root) {
        PrKitPlace place{.height = prKitPx(120), .flex = PrKitFlex_Fixed};
        PrLayoutNode split = prKitSplit(direct.kit, root, {.count = 2, .weights = weights},
            {.direction = PrKitDirection_Row, .place = place});
        prKitLabel(direct.kit, prKitSplitPane(direct.kit, split, 0), {.text = "First"}, {});
        prKitLabel(direct.kit, prKitSplitPane(direct.kit, split, 1), {.text = "Second"}, {});
        PrKitBoxStyle box{.overflow = PrKitOverflow_Clip, .place = {.height = prKitPx(40), .flex = PrKitFlex_Fixed}};
        prKitBox(direct.kit, root, {}, box);
        prKitLabel(direct.kit, root, {.text = "After"}, {});
    };
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // Without its weights, each pane weighs one.
    scripted.run("kitTree.set(split, 'weights', null);");
    weights[0] = 1;
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
}

TEST_CASE("a split refuses a value that is no weight, share or direction, and keeps what it had") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kSplitScript);
    scripted.run(R"JS(
        var refused = [
            kitTree.set(split, 'weights', '1 heavy'), kitTree.set(split, 'weights', '1 -3'),
            kitTree.set(split, 'min-share', '1.5'), kitTree.set(split, 'min-share', 'half'),
            kitTree.set(split, 'direction', 'diagonal'), kitTree.set(split, 'layout', '{}'),
            kitTree.set(panes[0], 'weights', '1'), kitTree.listen(split, 'click', true),
            kitTree.listen(panes[0], 'change', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("refused") == 0);
    DirectSplit direct;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
}

TEST_CASE("a dock that a script lays out renders as the kit's model of the same arrangement does") {
    DockPair pair;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.fixture.paintedCommands());
    // Nothing that the script did not ask for reaches it.
    CHECK(pair.scripted.number("heard.length") == 0);

    // A dock that its script places sits as the same placement of the kit's dock does.
    pair.scripted.run("kitTree.set(dock, 'margin', '12'); kitTree.set(dock, 'flex', 'fill');");
    pair.direct.placed = true;
    pair.frame();
    CHECK(pair.alike());

    // A pane's text and its flag are what its tab shows.
    pair.scripted.run(R"JS(
        kitTree.setText(panes.scene, 'Stage');
        kitTree.set(panes.viewport, 'dirty', 'true');
        kitTree.set(panes.console, 'dirty', null);
    )JS");
    pair.direct.tabs.titles[Panel_Scene] = "Stage";
    pair.direct.tabs.dirty[Panel_Viewport] = true;
    pair.direct.tabs.dirty[Panel_Console] = false;
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard.length") == 0);
}

TEST_CASE("a dragged band of a dock tells the script the layout with the new weights, which stand") {
    DockPair pair;
    pair.frame();
    PrRect scene = pair.direct.content(Panel_Scene);
    PrRect viewport = pair.direct.content(Panel_Viewport);
    PrVec2 band{(scene.right + viewport.left) / 2, 300};
    drag(pair.scripted.fixture, band, PrVec2{band.x + 79, band.y});
    drag(pair.direct.fixture, band, PrVec2{band.x + 79, band.y});
    PrKitDockNode root = prKitDockRoot(pair.direct.dock);
    CHECK(prKitDockSplitWeight(pair.direct.dock, root, 0) > 0.3f);
    CHECK(pair.scripted.number("count(dock, 'change')") >= 1);
    const char* const weights[] = {
        "shown().children[0].weight", "shown().children[1].weight", "shown().children[2].weight",
    };
    for (uint32_t i = 0; i < 3; ++i)
        CHECK(float(pair.scripted.number(weights[i])) == prKitDockSplitWeight(pair.direct.dock, root, i));
    CHECK(pair.scripted.number("shown().direction === 'row' && shown().weight === undefined ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[0].children[0].primary === true ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());

    // A pane that arrives brings the model in step with the layout again, which is the one that the drag left.
    pair.scripted.run("var late = make('dock-pane', dock, {name: 'late'}, 'Late');");
    pair.frame();
    CHECK(pair.alike());
}

TEST_CASE("a script that sets the layout that a dock told it changes nothing, and another layout rearranges the dock") {
    DockPair pair;
    pair.scripted.run(R"JS(
        function answer(node, type, detail) {
            if (node === dock && type === 'change')
                kitTree.set(dock, 'layout', detail.text);
        }
    )JS");
    pair.frame();
    PrRect scene = pair.direct.content(Panel_Scene);
    PrRect project = pair.direct.content(Panel_Project);
    PrVec2 band{(scene.left + scene.right) / 2, (scene.bottom + project.top - 22) / 2};
    drag(pair.scripted.fixture, band, PrVec2{band.x, band.y + 60});
    drag(pair.direct.fixture, band, PrVec2{band.x, band.y + 60});
    CHECK(pair.scripted.number("count(dock, 'change')") >= 1);
    CHECK(pair.scripted.number("shown().children[0].children[0].weight") > 0.55);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());

    // A layout that differs from the model in its weights alone gives the model those weights.
    uint32_t changes = uint32_t(pair.scripted.number("count(dock, 'change')"));
    pair.scripted.run(R"JS(
        var weighed = shown();
        [1, 1, 2].forEach(function (weight, index) { weighed.children[index].weight = weight; });
        kitTree.set(dock, 'layout', JSON.stringify(weighed));
    )JS");
    const float columns[] = {1, 1, 2};
    prKitDockSplitSetWeights(pair.direct.dock, prKitDockRoot(pair.direct.dock), columns, 3);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    // The script moves Console beside Inspector, and opens it, as the kit's own call does.
    pair.scripted.run(R"JS(
        var moved = weighed;
        moved.children[1] = {weight: moved.children[1].weight, tabs: ['viewport']};
        moved.children[2].tabs.push('console');
        moved.children[2].active = 'console';
        kitTree.set(dock, 'layout', JSON.stringify(moved));
    )JS");
    prKitDockMoveToTab(pair.direct.dock, Panel_Console, prKitDockPanelLeaf(pair.direct.dock, Panel_Inspector), 1);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    // A layout that the script sets is not told back to it.
    CHECK(pair.scripted.number("count(dock, 'change')") == changes);
}

TEST_CASE("a click on a tab of a dock opens its pane, and tells the script the layout with that tab active") {
    const char* layout = R"JS(
        var layout = {direction: 'row', children: [
            {tabs: ['scene']}, {tabs: ['inspector', 'notes'], active: 'notes', primary: true},
        ]};
    )JS";
    DockPair pair(layout);
    PrKitDockNode leaf = prKitDockPrimary(pair.direct.dock);
    prKitDockAddPanel(pair.direct.dock, leaf, Panel_Inspector);
    prKitDockAddPanel(pair.direct.dock, leaf, Panel_Notes);
    prKitDockSplit(pair.direct.dock, leaf, PrKitDockEdge_Left, Panel_Scene, 0.5f);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    // The first tab of the second leaf, which is not the open one.
    PrVec2 tab = pair.direct.tab(Panel_Notes);
    pair.scripted.fixture.click(tab);
    pair.direct.fixture.click(tab);
    pair.frame();
    CHECK(prKitDockLeafActiveTab(pair.direct.dock, leaf) == 0);
    CHECK(pair.scripted.number("count(dock, 'change')") == 1);
    CHECK(pair.scripted.number("shown().children[1].active === 'inspector' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[1].tabs.join() === 'inspector,notes' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[1].primary === true ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[0].primary === undefined ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());
}

TEST_CASE("closing a tab of a dock asks the script to remove its pane, and the dock holds the pane no more") {
    DockPair pair;
    pair.scripted.run(R"JS(
        function answer(node, type) {
            if (type === 'remove')
                kitTree.destroy(node);
        }
    )JS");
    pair.frame();
    PrVec2 tab = pair.direct.tab(Panel_Inspector);
    pair.scripted.fixture.move(tab);
    pair.direct.fixture.move(tab);
    pair.frame();
    PrVec2 close = pair.direct.closeButton(Panel_Inspector);
    REQUIRE(close.x > 0);
    pair.scripted.fixture.click(close);
    pair.direct.fixture.click(close);
    pair.frame();
    CHECK(prKitDockPanelLeaf(pair.direct.dock, Panel_Inspector) == kPrKitDockNodeNone);
    CHECK(pair.scripted.number("count(panes.inspector, 'remove')") == 1);
    CHECK(pair.scripted.number("heard.filter(function (event) { return event.type === 'remove'; }).length") == 1);
    CHECK(pair.scripted.number("count(dock, 'change')") == 1);
    // The pane's request comes before the dock's layout, which has two columns left.
    CHECK(pair.scripted.number("heard[0].type === 'remove' && heard[1].type === 'change' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children.length") == 2);
    CHECK(pair.scripted.number("JSON.stringify(shown()).indexOf('inspector')") == -1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());
}

TEST_CASE("a pane whose tab was closed stays out of a dock until its script puts it back in the layout") {
    DockPair pair;
    pair.frame();
    PrVec2 tab = pair.direct.tab(Panel_Inspector);
    pair.scripted.fixture.move(tab);
    pair.direct.fixture.move(tab);
    pair.frame();
    PrVec2 close = pair.direct.closeButton(Panel_Inspector);
    pair.scripted.fixture.click(close);
    pair.direct.fixture.click(close);
    pair.frame();
    settle(pair.scripted.fixture, pair.direct.fixture);
    // The script keeps the pane, which no tab names.
    CHECK(pair.scripted.number("count(panes.inspector, 'remove')") == 1);
    CHECK(pair.alike());

    // The layout that the script began with holds the pane again.
    pair.scripted.run("kitTree.set(dock, 'layout', JSON.stringify(layout));");
    DirectDock restored;
    restored.seedEditor();
    pair.scripted.fixture.move(PrVec2{400, 590});
    restored.fixture.move(PrVec2{400, 590});
    settle(pair.scripted.fixture, restored.fixture);
    CHECK(jsKitRendersAlike(pair.scripted.fixture, restored.fixture));
    CHECK(kitVerified(pair.scripted.fixture.tree));
}

TEST_CASE("a tab dragged onto another leaf of a dock joins its tabs, and the script hears where it went") {
    DockPair pair;
    pair.frame();
    PrVec2 from = pair.direct.tab(Panel_Console);
    PrRect inspector = pair.direct.content(Panel_Inspector);
    PrVec2 to{(inspector.left + inspector.right) / 2, (inspector.top + inspector.bottom) / 2};
    drag(pair.scripted.fixture, from, to);
    drag(pair.direct.fixture, from, to);
    PrKitDockNode leaf = prKitDockPanelLeaf(pair.direct.dock, Panel_Inspector);
    CHECK(prKitDockPanelLeaf(pair.direct.dock, Panel_Console) == leaf);
    CHECK(pair.scripted.number("count(dock, 'change')") == 1);
    CHECK(pair.scripted.number("heard.length") == 1);
    // Console's column gave way to Viewport's leaf, and Console is the open tab beside Inspector.
    CHECK(pair.scripted.number("shown().children.length") == 3);
    CHECK(pair.scripted.number("shown().children[1].tabs.join() === 'viewport' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[1].weight") == 0.5);
    CHECK(pair.scripted.number("shown().children[2].tabs.join() === 'inspector,console' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[2].active === 'console' ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());
}

TEST_CASE("a tab dropped near an edge of a leaf or of a dock splits there, and the script hears the new split") {
    DockPair pair;
    pair.frame();
    // Scene, dropped in the lower part of Inspector's leaf, takes the lower half of it.
    PrVec2 from = pair.direct.tab(Panel_Scene);
    PrRect inspector = pair.direct.content(Panel_Inspector);
    PrVec2 to{(inspector.left + inspector.right) / 2, inspector.bottom - 70};
    drag(pair.scripted.fixture, from, to);
    drag(pair.direct.fixture, from, to);
    PrKitDockNode split = prKitDockNodeParent(pair.direct.dock, prKitDockPanelLeaf(pair.direct.dock, Panel_Scene));
    CHECK(prKitDockSplitDirection(pair.direct.dock, split) == PrKitDirection_Column);
    CHECK(pair.scripted.number("count(dock, 'change')") == 1);
    CHECK(pair.scripted.number("shown().children[2].direction === 'column' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[2].children[0].tabs.join() === 'inspector' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[2].children[1].tabs.join() === 'scene' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[2].children[1].weight") == 0.5);
    // The primary leaf, which Scene left, stays with no tabs.
    CHECK(pair.scripted.number("shown().children[0].children[0].tabs.length") == 0);
    CHECK(pair.scripted.number("shown().children[0].children[0].primary === true ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());

    // Viewport, dropped near the dock's lower side, takes that side of the whole dock.
    from = pair.direct.tab(Panel_Viewport);
    drag(pair.scripted.fixture, from, PrVec2{400, 590});
    drag(pair.direct.fixture, from, PrVec2{400, 590});
    PrKitDockNode root = prKitDockRoot(pair.direct.dock);
    CHECK(prKitDockSplitDirection(pair.direct.dock, root) == PrKitDirection_Column);
    CHECK(pair.scripted.number("count(dock, 'change')") == 2);
    CHECK(pair.scripted.number("shown().direction === 'column' && shown().children.length === 2 ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[0].direction === 'row' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[1].tabs.join() === 'viewport' ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());
}

TEST_CASE("a dock brings a layout that its model cannot hold to the nearest arrangement that it can") {
    // A split in a split along the same axis, a split of one child, a leaf with no tabs, a tab that names no pane, a
    // tab that names a pane again, and a leaf that names its open tab by a name that is not among its tabs.
    const char* layout = R"JS(
        var layout = {direction: 'row', children: [
            {weight: 1, direction: 'row', children: [
                {weight: 1, tabs: ['scene', 'missing']}, {weight: 3, tabs: ['project', 'scene'], active: 'nothing'},
            ]},
            {weight: 1, tabs: []},
            {weight: 2, direction: 'column', children: [{tabs: ['viewport', 'console'], active: 'console'}]},
            {weight: 0, direction: 'column', children: []},
        ]};
    )JS";
    DockPair pair(layout);
    // Three leaves in a row: Scene with an eighth, Project with three eighths and the weight of the leaf that was left
    // out, and Viewport with Console, which is open, with a quarter.
    PrKitDockNode scene = prKitDockPrimary(pair.direct.dock);
    prKitDockAddPanel(pair.direct.dock, scene, Panel_Scene);
    PrKitDockNode project = prKitDockSplit(pair.direct.dock, scene, PrKitDockEdge_Right, Panel_Project, 0.5f);
    PrKitDockNode viewport = prKitDockSplit(pair.direct.dock, project, PrKitDockEdge_Right, Panel_Viewport, 0.5f);
    prKitDockAddPanel(pair.direct.dock, viewport, Panel_Console);
    const float weights[] = {0.25f, 1.75f, 2};
    prKitDockSplitSetWeights(pair.direct.dock, prKitDockRoot(pair.direct.dock), weights, 3);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard.length") == 0);
}

TEST_CASE("a dock gives the weight of a leaf that it leaves out to a neighbor, and equal shares to weights of zero") {
    // The first leaf names no pane, so the leaf after it takes its weight, and the split of two weightless leaves gives
    // each of them half of its own.
    const char* layout = R"JS(
        var layout = {direction: 'row', children: [
            {weight: 3, tabs: ['missing']},
            {weight: 1, tabs: ['scene']},
            {weight: 4, direction: 'row', children: [{weight: 0, tabs: ['viewport']}, {weight: 0, tabs: ['inspector']}]},
        ]};
    )JS";
    DockPair pair(layout);
    PrKitDockNode scene = prKitDockPrimary(pair.direct.dock);
    prKitDockAddPanel(pair.direct.dock, scene, Panel_Scene);
    PrKitDockNode viewport = prKitDockSplit(pair.direct.dock, scene, PrKitDockEdge_Right, Panel_Viewport, 0.5f);
    prKitDockSplit(pair.direct.dock, viewport, PrKitDockEdge_Right, Panel_Inspector, 0.5f);
    const float weights[] = {4, 2, 2};
    prKitDockSplitSetWeights(pair.direct.dock, prKitDockRoot(pair.direct.dock), weights, 3);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    pair.scripted.run(R"JS(
        layout.children.shift();
        layout.children[1] = {weight: 0, tabs: ['viewport']};
        layout.children.push({weight: 0, tabs: ['inspector']});
        layout.children[0].weight = 0;
        kitTree.set(dock, 'layout', JSON.stringify(layout));
    )JS");
    const float equal[] = {1, 1, 1};
    prKitDockSplitSetWeights(pair.direct.dock, prKitDockRoot(pair.direct.dock), equal, 3);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard.length") == 0);
}

TEST_CASE("a dock's primary leaf is the one that its layout marks, and it stays when it holds no tab") {
    const char* layout = R"JS(
        var layout = {direction: 'column', children: [
            {weight: 3, tabs: ['viewport']}, {weight: 1, primary: true}, {tabs: ['console', 'notes']},
        ]};
    )JS";
    DockPair pair(layout);
    PrKitDockNode primary = prKitDockPrimary(pair.direct.dock);
    PrKitDockNode viewport = prKitDockSplit(pair.direct.dock, primary, PrKitDockEdge_Top, Panel_Viewport, 0.5f);
    PrKitDockNode console = prKitDockSplit(pair.direct.dock, primary, PrKitDockEdge_Bottom, Panel_Console, 0.5f);
    prKitDockAddPanel(pair.direct.dock, console, Panel_Notes);
    prKitDockLeafSetActiveTab(pair.direct.dock, console, 0);
    const float weights[] = {3, 1, 1};
    prKitDockSplitSetWeights(pair.direct.dock, prKitDockRoot(pair.direct.dock), weights, 3);
    CHECK(prKitDockNodeIsValid(pair.direct.dock, viewport));
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    // A tab dropped on the empty leaf fills it, and the layout says that the leaf is still the primary one.
    PrVec2 from = pair.direct.tab(Panel_Viewport);
    drag(pair.scripted.fixture, from, PrVec2{400, 420});
    drag(pair.direct.fixture, from, PrVec2{400, 420});
    CHECK(prKitDockPanelLeaf(pair.direct.dock, Panel_Viewport) == primary);
    CHECK(pair.scripted.number("shown().children.length") == 2);
    CHECK(pair.scripted.number("shown().children[0].tabs.join() === 'viewport' ? 1 : 0") == 1);
    CHECK(pair.scripted.number("shown().children[0].primary === true ? 1 : 0") == 1);
    settle(pair.scripted.fixture, pair.direct.fixture);
    CHECK(pair.alike());
}

TEST_CASE("a dock with no layout is one empty leaf, and a pane shows once a layout names it") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run("var layout = {tabs: ['viewport', 'scene'], active: 'scene'};");
    scripted.run(kDockScript);
    scripted.run("kitTree.set(dock, 'layout', null);");
    DirectDock direct;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));

    scripted.run("kitTree.set(dock, 'layout', JSON.stringify(layout));");
    PrKitDockNode leaf = prKitDockPrimary(direct.dock);
    prKitDockAddPanel(direct.dock, leaf, Panel_Viewport);
    prKitDockAddPanel(direct.dock, leaf, Panel_Scene);
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));

    // A pane that is destroyed, or that takes another name, leaves the dock, and one that takes a tab's name joins it.
    scripted.run(R"JS(
        kitTree.destroy(panes.viewport);
        kitTree.set(panes.scene, 'name', 'stage');
        kitTree.set(panes.notes, 'name', 'viewport');
    )JS");
    prKitDockRemovePanel(direct.dock, Panel_Viewport);
    prKitDockRemovePanel(direct.dock, Panel_Scene);
    prKitDockAddPanel(direct.dock, leaf, Panel_Notes);
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    CHECK(kitVerified(scripted.fixture.tree));
    CHECK(scripted.number("heard.length") == 0);
}

TEST_CASE("a tab of one dock that is dragged over another shows its title there, and a drop there moves nothing") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run(R"JS(
        var docks = [['scene', 'project'], ['console', 'notes']].map(function (names) {
            const dock = make('dock', 0, {layout: JSON.stringify({tabs: names})});
            names.forEach(function (name) {
                const title = name[0].toUpperCase() + name.slice(1);
                make('label', make('dock-pane', dock, {name: name}, title), {}, title);
            });
            kitTree.listen(dock, 'change', true);
            return dock;
        });
    )JS");
    KitFixture direct;
    Tabs tabs;
    tabs.dirty[Panel_Console] = false;
    PrKitDock* docks[2] = {prKitDockCreate(), prKitDockCreate()};
    const PrKitDockPanelId held[2][2] = {{Panel_Scene, Panel_Project}, {Panel_Console, Panel_Notes}};
    for (uint32_t i = 0; i < 2; ++i) {
        PrKitDockNode leaf = prKitDockPrimary(docks[i]);
        prKitDockAddPanel(docks[i], leaf, held[i][0]);
        prKitDockAddPanel(docks[i], leaf, held[i][1]);
        prKitDockLeafSetActiveTab(docks[i], leaf, 0);
    }
    PrLayoutNode contents[2] = {};
    direct.build = [&](PrLayoutNode root) {
        for (uint32_t i = 0; i < 2; ++i) {
            prKitDockBuild(direct.kit, root, {.dock = docks[i], .describe = describe, .context = &tabs});
            PrKitDockPane pane = prKitDockPane(docks[i], 0);
            contents[i] = pane.content;
            prKitLabel(direct.kit, pane.content, {.text = tabs.titles[pane.panel]}, {});
        }
    };
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // Scene's tab, in flight over the second dock: both docks show its title by the pointer.
    PrRect first = direct.bounds(contents[0]);
    PrVec2 from{first.left + 10, first.top - 11};
    PrVec2 to = direct.center(contents[1]);
    KitFixture* both[] = {&scripted.fixture, &direct};
    for (KitFixture* f : both) {
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = from}});
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
        f->frame();
        f->move(PrVec2{from.x + 10, from.y + 10});
        f->frame();
        f->move(to);
        f->frame();
        f->frame();
    }
    REQUIRE(prUiDragging(direct.ui) != nullptr);
    REQUIRE(prUiDragging(scripted.fixture.ui) != nullptr);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    for (KitFixture* f : both) {
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = to}});
        f->frame();
    }
    settle(scripted.fixture, direct);
    CHECK(prKitDockLeafTabCount(docks[0], prKitDockPrimary(docks[0])) == 2);
    CHECK(prKitDockLeafTabCount(docks[1], prKitDockPrimary(docks[1])) == 2);
    CHECK(scripted.number("heard.length") == 0);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    prKitDockDestroy(docks[0]);
    prKitDockDestroy(docks[1]);
}

TEST_CASE("a script's dock shows no title for a dragged tab of a dock that its script did not make") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run(R"JS(
        var dock = make('dock', 0, {layout: JSON.stringify({tabs: ['scene']})});
        make('label', make('dock-pane', dock, {name: 'scene'}, 'Scene'), {}, 'Scene');
        var stray = make('label', 0, {}, 'Wrong');
        kitTree.listen(dock, 'change', true);
    )JS");
    // A dock of the kit's own calls in the script's kit. Its panel has the number of a node of the script that is no
    // pane, which the script's dock is asked to describe while the panel's tab is dragged.
    PrKitDockPanelId foreign = PrKitDockPanelId(scripted.number("stray"));
    REQUIRE(foreign != Panel_Scene);
    PrKitDock* native = prKitDockCreate();
    prKitDockAddPanel(native, prKitDockPrimary(native), foreign);
    scripted.fixture.build = [&](PrLayoutNode root) {
        prJsKitTreeBuild(scripted.tree, scripted.fixture.kit, root);
        prJsKitTreeDispatch(scripted.tree);
        prKitDockBuild(scripted.fixture.kit, root, {.dock = native, .describe = describeNative});
    };

    KitFixture direct;
    PrKitDock* docks[2] = {prKitDockCreate(), prKitDockCreate()};
    prKitDockAddPanel(docks[0], prKitDockPrimary(docks[0]), Panel_Scene);
    prKitDockAddPanel(docks[1], prKitDockPrimary(docks[1]), foreign);
    PrLayoutNode contents[2] = {};
    direct.build = [&](PrLayoutNode root) {
        prKitDockBuild(direct.kit, root, {.dock = docks[0], .describe = describeScene});
        contents[0] = prKitDockPane(docks[0], 0).content;
        prKitLabel(direct.kit, contents[0], {.text = "Scene"}, {});
        prKitLabel(direct.kit, root, {.text = "Wrong"}, {});
        prKitDockBuild(direct.kit, root, {.dock = docks[1], .describe = describeNative});
        contents[1] = prKitDockPane(docks[1], 0).content;
    };
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    PrRect second = direct.bounds(contents[1]);
    PrVec2 from{second.left + 10, second.top - 11};
    PrVec2 to = direct.center(contents[0]);
    KitFixture* both[] = {&scripted.fixture, &direct};
    for (KitFixture* f : both) {
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = from}});
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
        f->frame();
        f->move(PrVec2{from.x + 10, from.y - 10});
        f->frame();
        f->move(to);
        f->frame();
        f->frame();
    }
    REQUIRE(prUiDragging(scripted.fixture.ui) != nullptr);
    CHECK(prUiDragging(scripted.fixture.ui)->value == foreign);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The drop asks the script's dock to take a panel that is not its own, which it does not.
    for (KitFixture* f : both) {
        f->push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = to}});
        f->frame();
    }
    settle(scripted.fixture, direct);
    CHECK(prKitDockLeafTabCount(native, prKitDockPrimary(native)) == 1);
    CHECK(scripted.number("heard.length") == 0);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    prKitDockDestroy(native);
    prKitDockDestroy(docks[0]);
    prKitDockDestroy(docks[1]);
}

TEST_CASE("a dock refuses a layout that is not one, and keeps the layout that it had") {
    DockPair pair;
    pair.scripted.run(R"JS(
        var refused = [
            '', 'row', '[]', '{"tabs": "scene"}', '{"tabs": [1]}', '{"tabs": [], "active": 1}',
            '{"tabs": [], "primary": "yes"}', '{"tabs": [], "weight": -1}', '{"tabs": [], "weight": "1"}',
            '{"children": []}', '{"direction": "diagonal", "children": []}', '{"direction": "row", "children": {}}',
            '{"direction": "row", "children": [{"tabs": ["scene"]}, 3]}', '{"tabs": ["scene"]',
        ].filter(function (text) { return kitTree.set(dock, 'layout', text); }).length;
        refused += [
            kitTree.set(dock, 'weights', '1 2'), kitTree.set(panes.scene, 'layout', '{}'),
            kitTree.set(panes.scene, 'dirty', 'very'), kitTree.listen(dock, 'remove', true),
            kitTree.listen(panes.scene, 'change', true),
        ].filter(Boolean).length;
        var taken = [
            kitTree.set(panes.scene, 'dirty', 'true'), kitTree.set(panes.scene, 'dirty', null),
            kitTree.set(panes.scene, 'name', null), kitTree.set(panes.scene, 'name', 'scene'),
        ].filter(Boolean).length;
    )JS");
    CHECK(pair.scripted.number("refused") == 0);
    CHECK(pair.scripted.number("taken") == 4);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
}
