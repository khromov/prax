// The kinds of Studio Kit's collections in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/draw_image.h"
#include "pr/format.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_collection.h"
#include "pr/kit_text.h"
#include "pr/layout_tree.h"
#include "pr/string.h"
#include "pr/ui.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"
#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// The heights of the rows of the tests' lists, trees and tables, and the height of the views that show them, which is
// ten rows of a list.
constexpr float kRowHeight = 22;
constexpr float kTreeRowHeight = 24;
constexpr float kTableRowHeight = 26;
constexpr float kViewHeight = 220;

// The payload kind of the tests' tree rows.
constexpr uint64_t kRowKind = 7;

// What every test's script begins with, after kJsKitMake. It keeps the events that the script hears in `events`, and
// hands each to `heard`, if a test sets it. `told` writes an event as its type, its text and its values. `applyEdit`
// applies a view's edit of its selection to the rows that the view shows, as a script that listens for `select` does.
// `entities` makes a list of rows, each with an icon, a label that numbers it and a meta text.
const char* const kHeard = R"JS(
    var events = [];
    var heard = null;
    kitTree.onevent = function (node, type, detail) {
        events.push({node: node, type: type, detail: detail});
        if (heard !== null)
            heard(node, type, detail);
    };
    function told(index) {
        var event = events[index];
        if (event === undefined)
            return 'nothing';
        var parts = [event.type];
        if (event.detail.text !== undefined)
            parts.push(event.detail.text);
        if (event.detail.values !== undefined)
            parts.push(event.detail.values.join(','));
        return parts.join(' ');
    }
    var chosen = {};
    function choose(row, value) {
        if (!!chosen[row] !== value) {
            chosen[row] = value;
            kitTree.set(row, 'selected', value ? '' : null);
        }
    }
    function applyEdit(rows, detail) {
        var first = detail.values[0];
        var end = Math.min(first + detail.values[1], rows.length);
        if (detail.text === 'replace') {
            for (var row in chosen)
                choose(Number(row), false);
        }
        for (var i = first; i < end; i++)
            choose(rows[i], detail.text === 'toggle' ? !chosen[rows[i]] : true);
    }
    function entity(view, id) {
        var row = make('list-row', view, {});
        make('row-icon', row, {icon: 'cube'});
        make('row-label', row, {tone: 'primary'}, 'Entity ' + id);
        make('row-meta', row, {}, 'mesh');
        return row;
    }
    function entities(count, attributes) {
        var view = make('list-view', 0, attributes);
        var rows = [];
        for (var i = 0; i < count; i++)
            rows.push(entity(view, i));
        return {view: view, rows: rows};
    }
)JS";

// A kit that a script drives and a kit that the kit's own calls drive, which take the same input.
struct Pair {
    JsKitScripted scripted;
    KitFixture direct;

    Pair() {
        scripted.run(kJsKitMake);
        scripted.run(kHeard);
    }

    void frame(uint32_t count = 1) {
        for (uint32_t i = 0; i < count; ++i) {
            scripted.fixture.frame();
            direct.frame();
        }
    }

    // Give both kits an event, or a click, a key or a move of the pointer, and the last three run a frame of each.
    void push(const PrAppEvent& event) {
        scripted.fixture.push(event);
        direct.push(event);
    }
    void click(PrVec2 at, PrAppModifierFlags modifiers = 0) {
        scripted.fixture.click(at, modifiers);
        direct.click(at, modifiers);
        frame();
    }
    void key(PrAppKey key, PrAppModifierFlags modifiers = 0) {
        scripted.fixture.key(key, modifiers);
        direct.key(key, modifiers);
        frame();
    }
    void move(PrVec2 at) {
        scripted.fixture.move(at);
        direct.move(at);
        frame();
    }

    // Returns the string that the expression `source` evaluates to in the script's realm.
    String text(const char* source) {
        JSContext* ctx = scripted.ctx;
        JSValue result = JS_Eval(ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
        size_t size = 0;
        const char* chars = JS_ToCStringLen(ctx, &size, result);
        String text;
        if (chars != nullptr)
            text = String(PrStringView{chars, size});
        JS_FreeCString(ctx, chars);
        JS_FreeValue(ctx, result);
        return text;
    }

    // Returns the event at `index` of those that the script heard, as `told` writes it.
    String told(uint32_t index) {
        char source[32] = {};
        formatToBuffer(Span<char>(source, countOf(source) - 1), "told({})", index);
        return text(source);
    }

    // Returns true if the expression `source` holds in the script's realm.
    bool holds(const char* source) {
        JSContext* ctx = scripted.ctx;
        JSValue result = JS_Eval(ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
        bool held = JS_ToBool(ctx, result) > 0;
        JS_FreeValue(ctx, result);
        return held;
    }

    // Runs the frames that let what the input started end, and requires that both kits then lay out as a layout from
    // scratch does and render the same pixels.
    void requireAlike(const char* when) {
        frame(30);
        INFO(String(PrStringView(when)));
        CHECK(kitVerified(scripted.fixture.tree));
        CHECK(kitVerified(direct.tree));
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }

    // Returns the first widget that the script's tree declared, which the tests' scripts make a view.
    PrLayoutNode scriptedView() {
        return prLayoutNodeFirstChild(scripted.fixture.tree, scripted.fixture.root);
    }
};

// Returns the number of rows that the list or tree view `view` declared: the children of its content, which is the
// view's first child.
uint32_t declaredRows(KitFixture& f, PrLayoutNode view) {
    uint32_t count = 0;
    PrLayoutNode content = prLayoutNodeFirstChild(f.tree, view);
    for (PrLayoutNode row = prLayoutNodeFirstChild(f.tree, content); row != kPrLayoutNodeNone;
            row = prLayoutNodeNextSibling(f.tree, row))
        ++count;
    return count;
}

// A row of the tests' lists: the number in its label, and whether it is selected.
struct Entity {
    uint32_t id;
    bool selected;
};

// The list that the script's `entities` makes, declared by the kit's own calls. The application holds each row's
// membership and applies the view's edits to it.
struct DirectList {
    KitFixture& f;
    Array<Entity> rows;
    bool attached = true;           // the list is declared
    PrKitSelection selection{};
    PrLayoutNode view = kPrLayoutNodeNone;
    PrKitWindow window{};
    Array<PrLayoutNode> shown;      // the rows of the window, in order

    DirectList(KitFixture& fixture, uint32_t count) : f(fixture) {
        for (uint32_t i = 0; i < count; ++i)
            rows.push(Entity{.id = i});
        f.build = [this](PrLayoutNode root) { build(root); };
    }

    void build(PrLayoutNode root) {
        if (!attached)
            return;
        PrKitListViewStyle style{
            .rowHeight = kRowHeight,
            .place = {.height = prKitPx(kViewHeight), .flex = PrKitFlex_Fixed},
        };
        view = prKitListView(f.kit, root, {.count = rows.count(), .selection = &selection}, style);
        window = prKitListWindow(f.kit, view);
        shown.clear();
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            const Entity& entity = rows[index];
            char label[32];
            PrLayoutNode row =
                prKitListRow(f.kit, view, {.index = index, .item = entity.id, .selected = entity.selected});
            prKitRowIcon(f.kit, row, {.icon = PrKitIconKind_Cube});
            prKitRowLabel(f.kit, row, {.text = formatToBuffer(Span(label), "Entity {}", entity.id)}, PrKitTone_Primary);
            prKitRowMeta(f.kit, row, {.text = "mesh"});
            shown.push(row);
        }
        PrKitSelectionEdit edit{};
        if (prKitSelectionEdited(f.kit, view, &edit) && rows.count() != 0) {
            Array<bool> flags;
            for (const Entity& entity : rows)
                flags.push(entity.selected);
            prKitSelectionApply(&edit, flags.data(), flags.count());
            for (uint32_t i = 0; i < rows.count(); ++i)
                rows[i].selected = flags[i];
        }
    }

    // Returns the center of row `index`, which is in the window.
    PrVec2 at(uint32_t index) {
        return f.center(shown[index - window.first]);
    }
};

// A row of the tests' trees, in display order: its name, where it is in the tree, and its state. `parent` gives the row
// a disclosure though no row under it follows. `eye` gives it a visibility toggle, which `pinned` reveals wherever the
// pointer is.
struct Branch {
    const char* name;
    uint32_t depth;
    bool open;
    bool parent;
    bool eye;
    bool pinned;
    bool hidden;
    bool selected;
};

// The tree that the tests' scripts make, declared by the kit's own calls. The application holds every row, flattens the
// rows that show in each round, and applies the view's edits and folds to its own storage.
struct DirectTree {
    KitFixture& f;
    Array<Branch> branches;
    uint64_t dragKind = 0;
    bool bound = true;              // the view edits `selection`
    PrKitSelection selection{};

    // What the last round showed, each with one element for each row that shows, in order: the row's place among the
    // branches, the row as the kit takes it, the row's node, and the node of its visibility toggle. A row outside the
    // window has no node, and a row with no visibility toggle has none for it.
    Array<uint32_t> shown;
    Array<PrKitTreeItem> items;
    Array<PrLayoutNode> nodes;
    Array<PrLayoutNode> toggles;

    PrLayoutNode view = kPrLayoutNodeNone;
    PrKitTreeDrop drop{};
    uint32_t drops = 0;

    explicit DirectTree(KitFixture& fixture) : f(fixture) {
        const Branch scene[] = {
            {.name = "Scene", .open = true, .eye = true},
            {.name = "Camera", .depth = 1},
            {.name = "Lights", .depth = 1, .parent = true},
            {.name = "Props", .eye = true, .pinned = true},
            {.name = "Crate", .depth = 1},
            {.name = "Barrel", .depth = 1},
            {.name = "Sky", .eye = true},
        };
        for (const Branch& branch : scene)
            branches.push(branch);
        f.build = [this](PrLayoutNode root) { build(root); };
    }

    // Flattens the rows that show: every row that is not under a closed one.
    void order() {
        constexpr uint32_t kNone = 0xffffffffu;
        shown.clear();
        items.clear();
        uint32_t closed = kNone;
        bool last = false;
        for (uint32_t i = 0; i < branches.count(); ++i) {
            const Branch& branch = branches[i];
            if (last && branch.depth > items.last().depth)
                items.last().hasChildren = true;
            last = closed == kNone || branch.depth <= closed;
            if (!last)
                continue;
            closed = branch.open ? kNone : branch.depth;
            shown.push(i);
            items.push(PrKitTreeItem{.depth = branch.depth, .hasChildren = branch.parent, .open = branch.open});
        }
    }

    void build(PrLayoutNode root) {
        order();
        PrKitTreeView tree{
            .count = items.count(),
            .selection = bound ? &selection : nullptr,
            .items = items.data(),
            .dragKind = dragKind,
        };
        PrKitListViewStyle style{
            .rowHeight = kTreeRowHeight,
            .place = {.height = prKitPx(kViewHeight), .flex = PrKitFlex_Fixed},
        };
        view = prKitTreeView(f.kit, root, tree, style);
        PrKitWindow window = prKitListWindow(f.kit, view);
        nodes.clear();
        toggles.clear();
        for (uint32_t i = 0; i < items.count(); ++i) {
            nodes.push(kPrLayoutNodeNone);
            toggles.push(kPrLayoutNodeNone);
        }
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            Branch& branch = branches[shown[index]];
            PrLayoutNode row =
                prKitTreeRow(f.kit, view, {.index = index, .item = shown[index], .selected = branch.selected});
            prKitRowIcon(f.kit, row, {.icon = PrKitIconKind_Cube});
            prKitRowLabel(f.kit, row, {.text = branch.name},
                PrKitTone(branch.hidden ? PrKitTone_Faint : PrKitTone_Primary));
            nodes[index] = row;
            if (!branch.eye)
                continue;
            PrKitVisibilityToggle toggle{
                .hidden = branch.hidden,
                .revealed = branch.pinned || prUiIsHovered(f.ui, row),
            };
            toggles[index] = prKitVisibilityToggle(f.kit, row, toggle);
            if (prKitActivated(f.kit, toggles[index]))
                branch.hidden = !branch.hidden;
        }
        PrKitSelectionEdit edit{};
        if (prKitSelectionEdited(f.kit, view, &edit)) {
            Array<bool> flags;
            if (edit.op == PrKitSelectionOp_Replace) {
                for (Branch& branch : branches)
                    branch.selected = false;
            }
            for (uint32_t row : shown)
                flags.push(branches[row].selected);
            prKitSelectionApply(&edit, flags.data(), flags.count());
            for (uint32_t i = 0; i < shown.count(); ++i)
                branches[shown[i]].selected = flags[i];
        }
        PrKitTreeFold fold{};
        if (prKitFolded(f.kit, view, &fold))
            branches[shown[fold.row]].open = fold.open;
        PrKitTreeDrop dropped{};
        if (prKitRowDropped(f.kit, view, &dropped)) {
            drop = dropped;
            ++drops;
        }
    }

    // Returns the disclosure of row `row`, which is the first cell of its row.
    PrLayoutNode disclosure(uint32_t row) {
        return prLayoutNodeFirstChild(f.tree, nodes[row]);
    }
};

// The script that makes the tree of DirectTree under a view with `attributes`, which a test's script names before it
// runs this. `model` holds every row in display order, and `showing` returns the rows that show, as the view counts
// them.
const char* const kTreeScript = R"JS(
    var model = [
        {name: 'Scene', depth: 0, open: true}, {name: 'Camera', depth: 1}, {name: 'Lights', depth: 1, parent: true},
        {name: 'Props', depth: 0, pinned: true}, {name: 'Crate', depth: 1}, {name: 'Barrel', depth: 1},
        {name: 'Sky', depth: 0},
    ];
    var view = make('tree-view', 0, attributes);
    model.forEach(function (item) {
        var set = {depth: String(item.depth)};
        if (item.open)
            set.open = '';
        if (item.parent)
            set['has-children'] = 'true';
        item.row = make('tree-row', view, set);
        make('row-icon', item.row, {icon: 'cube'});
        item.label = make('row-label', item.row, {tone: 'primary'}, item.name);
        if (item.depth === 0)
            item.toggle = make('visibility-toggle', item.row, {revealed: item.pinned ? '' : 'hover'});
    });
    function showing() {
        var rows = [];
        var closed = -1;
        model.forEach(function (item) {
            if (closed >= 0 && item.depth > closed)
                return;
            closed = item.open ? -1 : item.depth;
            rows.push(item);
        });
        return rows;
    }
)JS";

// The steps that the tests of a tree's folds take: a click on the disclosure of Props, a click on Scene, and the keys
// that close Scene, open it, step into it, move down to Lights and open that.
void foldAndWalk(Pair& pair, DirectTree& tree) {
    pair.click(pair.direct.center(tree.disclosure(3)));
    pair.click(pair.direct.center(tree.nodes[0]));
    pair.key(PrAppKey_Left);
    pair.key(PrAppKey_Right);
    pair.key(PrAppKey_Right);
    pair.key(PrAppKey_Down);
    pair.key(PrAppKey_Right);
}

// A table of `kAssets` rows in four columns, declared by the kit's own calls as the tests' script makes it. The
// application orders the rows, and holds each asset's membership.
struct DirectTable {
    static constexpr uint32_t kAssets = 30;
    KitFixture& f;
    PrKitTableSort sort{.column = 1, .descending = true};
    PrKitSelection selection{};
    bool selected[kAssets] = {};    // whether each asset is selected
    uint32_t order[kAssets] = {};   // the asset of each row
    bool reversed = false;          // the rows are in the descending order of their assets
    uint32_t sorts = 0;
    PrLayoutNode view = kPrLayoutNodeNone;
    PrKitWindow window{};
    Array<PrLayoutNode> shown;      // the rows of the window, in order

    explicit DirectTable(KitFixture& fixture) : f(fixture) {
        selected[6] = true;
        f.build = [this](PrLayoutNode root) { build(root); };
    }

    void build(PrLayoutNode root) {
        const PrKitTableColumn columns[] = {
            {.label = "NAME", .width = 120, .flex = PrKitFlex_Fill, .weight = 2, .sortable = true},
            {.label = "TYPE", .width = 80, .flex = PrKitFlex_Fill, .sortable = true},
            {.label = "SIZE", .width = 60, .align = PrKitTextAlign_End},
            {.label = "KIND", .width = 50, .align = PrKitTextAlign_Center},
        };
        PrKitTable table{
            .count = kAssets,
            .selection = &selection,
            .columns = columns,
            .columnCount = countOf(columns),
            .sort = &sort,
        };
        PrKitListViewStyle style{
            .rowHeight = kTableRowHeight,
            .place = {.width = prKitPx(500), .height = prKitPx(kViewHeight), .flex = PrKitFlex_Fixed},
        };
        view = prKitTable(f.kit, root, table, style);
        // The assets are in the order of their numbers, and the first column sorts by them.
        if (prKitSorted(f.kit, view)) {
            ++sorts;
            reversed = sort.column == 0 && sort.descending;
        }
        for (uint32_t i = 0; i < kAssets; ++i)
            order[i] = reversed ? kAssets - 1 - i : i;
        window = prKitListWindow(f.kit, view);
        shown.clear();
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            uint32_t asset = order[index];
            char label[32];
            PrLayoutNode row = prKitTableRow(f.kit, view, {.index = index, .item = asset, .selected = selected[asset]});
            PrKitTableCell name{
                .column = 0,
                .text = formatToBuffer(Span(label), "Entity {}", asset),
                .icon = PrKitIconKind_Cube,
                .role = PrKitTextRole_BodySmall,
            };
            PrLayoutNode cell = prKitTableCell(f.kit, row, name);
            if (asset == 2)
                prKitBadge(f.kit, cell, {.text = "NEW"}, {});
            prKitTableCell(f.kit, row,
                {.column = 1, .text = "mesh", .role = PrKitTextRole_Mono, .tone = PrKitTone_Secondary});
            prKitTableCell(f.kit, row,
                {.column = 2, .text = "12 KB", .role = PrKitTextRole_Mono, .tone = PrKitTone_Muted});
            prKitTableCell(f.kit, row, {.column = asset == 1 ? 2u : 3u, .text = "a"});
            if (asset == 3)
                prKitLabel(f.kit, row, {.text = "!"}, {});
            shown.push(row);
        }
        PrKitSelectionEdit edit{};
        if (prKitSelectionEdited(f.kit, view, &edit)) {
            bool flags[kAssets] = {};
            for (uint32_t i = 0; i < kAssets; ++i)
                flags[i] = selected[order[i]];
            prKitSelectionApply(&edit, flags, kAssets);
            for (uint32_t i = 0; i < kAssets; ++i)
                selected[order[i]] = flags[i];
        }
    }

    // Returns the center of row `index`, which is in the window.
    PrVec2 at(uint32_t index) {
        return f.center(shown[index - window.first]);
    }

    // Returns the center of the heading of `column`, which is the column's child of the header's row.
    PrVec2 heading(uint32_t column) {
        PrLayoutNode header = prLayoutNodeFirstChild(f.tree, view);
        PrLayoutNode node = prLayoutNodeFirstChild(f.tree, prLayoutNodeFirstChild(f.tree, header));
        for (uint32_t i = 0; i < column; ++i)
            node = prLayoutNodeNextSibling(f.tree, node);
        return f.center(node);
    }
};

// The script that makes the table of DirectTable. `assets` holds the row of each asset, and the script selects one of
// them.
const char* const kTableScript = R"JS(
    var table = make('table', 0, {
        'row-height': '26', width: '500', height: '220', flex: 'fixed', selectable: '', 'sort-column': '1',
        'sort-descending': '',
    });
    make('table-column', table, {width: '120', flex: 'fill', weight: '2', sortable: ''}, 'NAME');
    make('table-column', table, {width: '80', flex: 'fill', sortable: ''}, 'TYPE');
    make('table-column', table, {width: '60', align: 'end'}, 'SIZE');
    make('table-column', table, {width: '50', align: 'center'}, 'KIND');
    var assets = [];
    for (var id = 0; id < 30; id++) {
        var row = make('table-row', table, {});
        var name = make('table-cell', row, {icon: 'cube', role: 'body-small'}, 'Entity ' + id);
        if (id === 2)
            make('badge', name, {}, 'NEW');
        make('table-cell', row, {role: 'mono', tone: 'secondary'}, 'mesh');
        make('table-cell', row, {role: 'mono', tone: 'muted'}, '12 KB');
        make('table-cell', row, id === 1 ? {column: '2'} : {}, 'a');
        if (id === 3)
            make('label', row, {}, '!');
        assets.push(row);
    }
    choose(assets[6], true);
)JS";

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a list view's rows show their icons, labels and meta text, and a row tells its click") {
    Pair pair;
    pair.scripted.run(R"JS(
        var view = make('list-view', 0, {'row-height': '28', height: '200', flex: 'fixed', margin: '4 8'});
        var names = ['Camera', 'Sun', 'Terrain', 'Player', 'Enemy', 'Trigger'];
        var tones = [null, 'primary', 'secondary', 'faint', 'accent', 'danger'];
        var rows = names.map(function (name, index) {
            var row = make('list-row', view, index === 1 ? {selected: ''} : {});
            make('row-icon', row, {icon: index % 2 === 1 ? 'cube' : 'sphere'});
            make('row-label', row, tones[index] === null ? {} : {tone: tones[index]}, name);
            if (index !== 4)
                make('row-meta', row, {}, 'mesh ' + index);
            return row;
        });
        kitTree.listen(rows[3], 'click', true);
    )JS");
    const char* names[] = {"Camera", "Sun", "Terrain", "Player", "Enemy", "Trigger"};
    const PrKitTone tones[] = {
        PrKitTone_Auto, PrKitTone_Primary, PrKitTone_Secondary, PrKitTone_Faint, PrKitTone_Accent, PrKitTone_Danger,
    };
    PrLayoutNode nodes[6] = {};
    uint32_t clicks = 0;
    KitFixture& direct = pair.direct;
    direct.build = [&](PrLayoutNode root) {
        PrKitListViewStyle style{
            .rowHeight = 28,
            .place = {.height = prKitPx(200), .margin = prKitEdgesXY(4, 8), .flex = PrKitFlex_Fixed},
        };
        PrLayoutNode view = prKitListView(direct.kit, root, {.count = 6}, style);
        PrKitWindow window = prKitListWindow(direct.kit, view);
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            char meta[16];
            PrLayoutNode row = prKitListRow(direct.kit, view, {.index = index, .item = index, .selected = index == 1});
            PrKitIconKind icon = index % 2 == 1 ? PrKitIconKind_Cube : PrKitIconKind_Sphere;
            prKitRowIcon(direct.kit, row, {.icon = icon});
            prKitRowLabel(direct.kit, row, {.text = names[index]}, tones[index]);
            if (index != 4)
                prKitRowMeta(direct.kit, row, {.text = formatToBuffer(Span(meta), "mesh {}", index)});
            clicks += prKitActivated(direct.kit, row) ? 1 : 0;
            nodes[index] = row;
        }
    };
    pair.frame(2);
    pair.requireAlike("at rest");
    CHECK(pair.scripted.fixture.paintedCommands() == direct.paintedCommands());

    // The view selects nothing, so a click on a row is the row's alone, and takes no focus.
    pair.click(direct.center(nodes[3]));
    pair.click(direct.center(nodes[2]));
    CHECK(clicks == 2);
    CHECK(pair.scripted.number("events.length") == 1);
    CHECK(pair.told(0) == "click 3");
    CHECK(pair.holds("events[0].node === rows[3]"));
    CHECK(prUiFocus(pair.scripted.fixture.ui) == kPrLayoutNodeNone);
    pair.requireAlike("after the clicks");

    // A value that an attribute refuses changes nothing.
    CHECK_FALSE(pair.holds("kitTree.set(view, 'row-height', '-4') || kitTree.set(rows[0], 'selected', 'yes')"));
    pair.requireAlike("after the refused values");
}

TEST_CASE("a list of ten thousand rows declares the rows of its window alone, before and after it scrolls") {
    Pair pair;
    pair.scripted.run(R"JS(
        var list = entities(10000, {'row-height': '22', height: '220', flex: 'fixed', selectable: ''});
    )JS");
    DirectList list(pair.direct, 10000);
    pair.frame(2);
    pair.requireAlike("at rest");
    CHECK(declaredRows(pair.direct, list.view) < 20);
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) == declaredRows(pair.direct, list.view));

    PrVec2 at = pair.direct.center(list.view);
    pair.move(at);
    pair.push(PrAppEvent{
        .kind = PrAppEventKind_Scroll,
        .scroll = {.position = at, .delta = PrVec2{0, 50 * kRowHeight}},
    });
    pair.frame();
    CHECK(list.window.first > 40);
    pair.requireAlike("after the wheel");
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) == declaredRows(pair.direct, list.view));

    // No script listens, so the view applies its own edits: a click, then End, which scrolls to the last row, then a
    // range from there, and every row.
    pair.click(at);
    pair.requireAlike("after a click on a row that the wheel reached");
    pair.key(PrAppKey_End);
    CHECK(list.selection.lead == 9999);
    CHECK(list.window.first + list.window.count == 10000);
    pair.requireAlike("at the last row");
    pair.key(PrAppKey_Up, PrAppModifierFlag_Shift);
    pair.key(PrAppKey_PageUp, PrAppModifierFlag_Shift);
    pair.requireAlike("after a range");
    pair.key('A', PrAppModifierFlag_Super);
    pair.requireAlike("with every row selected");
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) < 20);
    CHECK(pair.scripted.number("events.length") == 0);
}

TEST_CASE("a view tells a script that listens of each edit of its selection, and the script applies it") {
    Pair pair;
    pair.scripted.run(R"JS(
        var list = entities(50, {'row-height': '22', height: '220', flex: 'fixed', selectable: ''});
        kitTree.listen(list.view, 'select', true);
        heard = function (node, type, detail) {
            if (node !== list.view || type !== 'select')
                throw new Error('an event of another node');
            applyEdit(list.rows, detail);
        };
    )JS");
    DirectList list(pair.direct, 50);
    pair.frame(2);

    // A click replaces the selection, Shift extends it from the anchor, the accelerator toggles a row, and both add a
    // range.
    pair.click(list.at(2));
    CHECK(pair.told(0) == "select replace 2,1,2,2");
    pair.click(list.at(5), PrAppModifierFlag_Shift);
    CHECK(pair.told(1) == "select replace 2,4,5,2");
    pair.click(list.at(3), PrAppModifierFlag_Super);
    CHECK(pair.told(2) == "select toggle 3,1,3,3");
    pair.click(list.at(7), PrAppModifierFlag_Control | PrAppModifierFlag_Shift);
    CHECK(pair.told(3) == "select add 3,5,7,3");
    CHECK(prUiFocus(pair.scripted.fixture.ui) == pair.scriptedView());
    pair.requireAlike("after the clicks");

    // The keys move the lead, extend the range, toggle the lead, and select every row.
    pair.key(PrAppKey_Down);
    CHECK(pair.told(4) == "select replace 8,1,8,8");
    pair.key(PrAppKey_Down, PrAppModifierFlag_Shift);
    CHECK(pair.told(5) == "select replace 8,2,9,8");
    pair.key(' ');
    CHECK(pair.told(6) == "select toggle 9,1,9,8");
    pair.requireAlike("after the keys");
    pair.key('A', PrAppModifierFlag_Super);
    CHECK(pair.told(7) == "select replace 0,50,49,0");
    CHECK(pair.scripted.number("events.length") == 8);
    pair.requireAlike("with every row selected");
}

TEST_CASE("a view that no script listens to applies the edits of its selection to its rows itself") {
    Pair pair;
    pair.scripted.run(R"JS(
        var list = entities(50, {'row-height': '22', height: '220', flex: 'fixed', selectable: ''});
        kitTree.set(list.rows[4], 'selected', '');
    )JS");
    DirectList list(pair.direct, 50);
    list.rows[4].selected = true;
    pair.frame(2);
    pair.requireAlike("with the row that the script selected");
    pair.click(list.at(2));
    pair.click(list.at(5), PrAppModifierFlag_Shift);
    pair.click(list.at(3), PrAppModifierFlag_Super);
    pair.click(list.at(8), PrAppModifierFlag_Control);
    pair.requireAlike("after the clicks");
    pair.key(PrAppKey_Down);
    pair.key(PrAppKey_Down, PrAppModifierFlag_Shift);
    pair.key(' ');
    pair.requireAlike("after the keys");

    // The cursor is the view's, and a script moves it by its attributes.
    CHECK(pair.holds("kitTree.set(list.view, 'lead', '3') && kitTree.set(list.view, 'anchor', '1')"));
    list.selection = PrKitSelection{.anchor = 1, .lead = 3};
    pair.key(PrAppKey_Down, PrAppModifierFlag_Shift);
    CHECK(list.selection.lead == 4);
    CHECK(list.rows[1].selected);
    CHECK(list.rows[4].selected);
    CHECK_FALSE(list.rows[5].selected);
    pair.requireAlike("after a range from the cursor that the script set");
    CHECK(pair.scripted.number("events.length") == 0);
}

TEST_CASE("rows that a script inserts, moves, removes and destroys keep their order and their selection in the view") {
    Pair pair;
    pair.scripted.run(R"JS(
        var list = entities(30, {'row-height': '22', height: '220', flex: 'fixed', selectable: ''});
    )JS");
    DirectList list(pair.direct, 30);
    pair.frame(2);
    pair.click(list.at(2));
    pair.requireAlike("with a row selected");

    // A new row goes first, the row of entity 5 goes last, and those of entities 1 and 3 go away. The selected row
    // keeps its selection where it now is.
    pair.scripted.run(R"JS(
        var added = entity(list.view, 100);
        kitTree.insert(list.view, added, list.rows[0]);
        kitTree.insert(list.view, list.rows[5], 0);
        kitTree.destroy(list.rows[1]);
        kitTree.remove(list.rows[3]);
    )JS");
    Entity moved = list.rows[5];
    list.rows.erase(5);
    list.rows.push(moved);
    list.rows.erase(3);
    list.rows.erase(1);
    list.rows.insert(0, Entity{.id = 100});
    pair.frame();
    CHECK(list.rows[2].id == 2);
    CHECK(list.rows[2].selected);
    pair.requireAlike("after the script's edits");

    // The keys count the rows as the script left them.
    pair.key(PrAppKey_Down);
    CHECK(list.rows[3].selected);
    pair.requireAlike("after a key");
    pair.key(PrAppKey_End);
    CHECK(list.rows[28].id == 5);
    CHECK(list.rows[28].selected);
    pair.requireAlike("at the row that the script moved last");

    // A view that is out of the tree while the script edits its rows shows them as they are once it is back.
    pair.scripted.run("kitTree.remove(list.view);");
    list.attached = false;
    pair.requireAlike("with the view out of the tree");
    pair.scripted.run(R"JS(
        kitTree.destroy(list.rows[0]);
        kitTree.set(list.rows[2], 'selected', null);
        kitTree.set(list.rows[4], 'selected', '');
        kitTree.insert(0, list.view, 0);
    )JS");
    REQUIRE(list.rows[1].id == 0);
    list.rows.erase(1);
    list.rows[1].selected = false;
    list.rows[2].selected = true;
    list.attached = true;
    pair.frame();
    CHECK(list.rows[2].id == 4);
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) == declaredRows(pair.direct, list.view));
    pair.requireAlike("with the view back in the tree");
}

TEST_CASE("a tree's version grows with each edit of its nodes, and with nothing else") {
    JsKitNodeTree* tree = jsKitTreeCreate();
    uint64_t version = jsKitTreeVersion(tree);
    // Returns true if the version grew since the last call.
    auto grew = [&] {
        uint64_t was = version;
        version = jsKitTreeVersion(tree);
        return version > was;
    };
    const JsKitKind* list = jsKitKindFrom("list-view");
    const JsKitKind* row = jsKitKindFrom("list-row");
    REQUIRE(list != nullptr);
    REQUIRE(row != nullptr);
    JsKitNodeIndex view = jsKitNodeCreate(tree, list);
    CHECK(grew());
    JsKitNodeIndex first = jsKitNodeCreate(tree, row);
    CHECK(grew());
    jsKitNodeInsert(tree, 0, view, kJsKitNodeNone);
    CHECK(grew());
    jsKitNodeInsert(tree, view, first, kJsKitNodeNone);
    CHECK(grew());
    CHECK(jsKitNodeSet(tree, first, "selected", "", false));
    CHECK(grew());
    CHECK(jsKitNodeSet(tree, view, "height", "120", false));
    CHECK(grew());
    jsKitNodeSetText(tree, first, "A row");
    CHECK(grew());

    // A value that an attribute refuses is no edit, and neither is listening or reading.
    CHECK_FALSE(jsKitNodeSet(tree, first, "selected", "yes", false));
    CHECK(jsKitNodeListen(tree, first, "click", true));
    CHECK(jsKitNodeParent(tree, first) == view);
    CHECK(jsKitTreeHas(tree, first));
    CHECK_FALSE(grew());

    jsKitNodeRemove(tree, first);
    CHECK(grew());
    jsKitNodeDestroy(tree, first);
    CHECK(grew());
    jsKitTreeDestroy(tree);
}

TEST_CASE("a grid's tiles show their icons and labels, select as rows do, and a tile tells its click") {
    Pair pair;
    pair.scripted.run(R"JS(
        var view = make('list-view', 0, {
            'row-height': '80', 'tile-width': '100', width: '410', height: '220', flex: 'fixed', selectable: '',
        });
        var tints = [{icon: 'cube', tint: 'accent-weak'}, {icon: 'sphere'}, {icon: 'image', tint: '0.27 0.4 0.33'}];
        var tiles = [];
        for (var i = 0; i < 40; i++) {
            var tile = make('list-tile', view, i === 1 ? {selected: ''} : {});
            make('tile-icon', tile, tints[i % 3]);
            make('tile-label', tile, {}, 'Tile ' + i);
            tiles.push(tile);
        }
        kitTree.listen(tiles[3], 'click', true);
    )JS");
    KitFixture& direct = pair.direct;
    bool selected[40] = {};
    selected[1] = true;
    PrKitSelection selection{};
    PrLayoutNode nodes[40] = {};
    direct.build = [&](PrLayoutNode root) {
        PrKitListViewStyle style{
            .rowHeight = 80,
            .tileWidth = 100,
            .place = {.width = prKitPx(410), .height = prKitPx(kViewHeight), .flex = PrKitFlex_Fixed},
        };
        PrLayoutNode view = prKitListView(direct.kit, root, {.count = 40, .selection = &selection}, style);
        PrKitWindow window = prKitListWindow(direct.kit, view);
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            char label[16];
            PrLayoutNode tile =
                prKitListTile(direct.kit, view, {.index = index, .item = index, .selected = selected[index]});
            if (index % 3 == 0)
                prKitTileIcon(direct.kit, tile, {.icon = PrKitIconKind_Cube}, direct.kit->theme.palette.accentWeak);
            else if (index % 3 == 1)
                prKitTileIcon(direct.kit, tile, {.icon = PrKitIconKind_Sphere}, PrColor{});
            else
                prKitTileIcon(direct.kit, tile, {.icon = PrKitIconKind_Image}, PrColor{0.27f, 0.4f, 0.33f, 1});
            prKitTileLabel(direct.kit, tile, {.text = formatToBuffer(Span(label), "Tile {}", index)});
            nodes[index] = tile;
        }
        PrKitSelectionEdit edit{};
        if (prKitSelectionEdited(direct.kit, view, &edit))
            prKitSelectionApply(&edit, selected, 40);
    };
    pair.frame(2);
    pair.requireAlike("at rest");

    pair.click(direct.center(nodes[2]));
    CHECK(selected[2]);
    CHECK_FALSE(selected[1]);
    pair.requireAlike("after a click on a tile");
    // Down moves the lead by a row of four tiles, and Right by a tile.
    pair.key(PrAppKey_Down);
    pair.key(PrAppKey_Right, PrAppModifierFlag_Shift);
    CHECK(selection.lead == 7);
    CHECK(selected[6]);
    pair.requireAlike("after the keys");
    pair.click(direct.center(nodes[3]));
    CHECK(pair.scripted.number("events.length") == 1);
    CHECK(pair.told(0) == "click 3");
    pair.requireAlike("after the click that the script heard");
}

TEST_CASE("a tile's image is the picture that its script gives, and grows under the pointer") {
    // Two rows of two pixels: red and green over blue and white.
    const uint8_t pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    PrDrawImageDesc desc{
        .pixels = {pixels, 8, {2, 2}},
        .format = PrPixelFormat_Rgba8,
        .alpha = PrDrawAlpha_Straight,
        .space = PrColorSpace_Srgb,
    };
    const PrDrawImage* image = nullptr;
    REQUIRE(prDrawImageCreate(&desc, &image) == PrStatus_Ok);
    {
        Pair pair;
        pair.scripted.run(R"JS(
            var view = make('list-view', 0, {
                'row-height': '80', 'tile-width': '100', width: '200', height: '200', flex: 'fixed',
            });
            var first = make('list-tile', view, {});
            var picture = make('tile-image', first, {image: '2 2 /wAA/wD/AP8AAP///////w==', 'hover-zoom': '0.5'});
            make('tile-label', first, {}, 'fox.png');
            var second = make('list-tile', view, {});
            make('tile-image', second, {});
            make('tile-label', second, {}, 'none.png');
        )JS");
        KitFixture& direct = pair.direct;
        PrLayoutNode tile = kPrLayoutNodeNone;
        bool pictured = true;
        direct.build = [&](PrLayoutNode root) {
            PrKitListViewStyle style{
                .rowHeight = 80,
                .tileWidth = 100,
                .place = {.width = prKitPx(200), .height = prKitPx(200), .flex = PrKitFlex_Fixed},
            };
            PrLayoutNode view = prKitListView(direct.kit, root, {.count = 2}, style);
            tile = prKitListTile(direct.kit, view, {.index = 0, .item = 0});
            if (pictured)
                prKitTileImage(direct.kit, tile, {.image = image}, {.hoverZoom = 0.5f});
            prKitTileLabel(direct.kit, tile, {.text = "fox.png"});
            // A tile whose image has no picture holds its label alone.
            PrLayoutNode bare = prKitListTile(direct.kit, view, {.index = 1, .item = 1});
            prKitTileLabel(direct.kit, bare, {.text = "none.png"});
        };
        pair.frame(2);
        pair.requireAlike("at rest");
        pair.move(direct.center(tile));
        pair.requireAlike("with the picture grown under the pointer");

        // A picture that the attribute refuses leaves the tile's picture as it was, and a reset takes it away.
        CHECK_FALSE(pair.holds("kitTree.set(picture, 'image', '2 2 /wAA/w==')"));
        pair.requireAlike("after a refused picture");
        CHECK(pair.holds("kitTree.set(picture, 'image', null)"));
        pictured = false;
        pair.requireAlike("with no picture");
        direct.build = {};
        direct.frame();
    }
    prDrawImageRelease(image);
}

TEST_CASE("an empty state shows its icon over its title and its hint") {
    Pair pair;
    pair.scripted.run(R"JS(
        make('empty-state', 0, {icon: 'search', hint: 'Clear the filter to see them all'}, 'NO ASSETS');
        var bare = make('empty-state', 0, {height: '120', flex: 'fixed'}, 'NOTHING HERE');
    )JS");
    KitFixture& direct = pair.direct;
    bool hinted = false;
    direct.build = [&](PrLayoutNode root) {
        prKitEmptyState(direct.kit, root,
            {.icon = PrKitIconKind_Search, .title = "NO ASSETS", .hint = "Clear the filter to see them all"});
        PrKitEmptyState bare{.title = "NOTHING HERE"};
        if (hinted)
            bare.hint = "Yet";
        prKitPlace(direct.kit, prKitEmptyState(direct.kit, root, bare),
            {.height = prKitPx(120), .flex = PrKitFlex_Fixed});
    };
    pair.frame(2);
    pair.requireAlike("at rest");
    pair.scripted.run("kitTree.set(bare, 'hint', 'Yet');");
    hinted = true;
    pair.requireAlike("with a hint that the script set later");
}

TEST_CASE("a tree view shows the rows under its open rows, and tells a script that listens of each fold") {
    Pair pair;
    pair.scripted.run("var attributes = {'row-height': '24', height: '220', flex: 'fixed', selectable: ''};");
    pair.scripted.run(kTreeScript);
    pair.scripted.run(R"JS(
        kitTree.listen(view, 'select', true);
        kitTree.listen(view, 'fold', true);
        heard = function (node, type, detail) {
            if (node !== view)
                throw new Error('an event of another node');
            if (type === 'fold') {
                var item = showing()[detail.values[0]];
                item.open = detail.values[1] === 1;
                kitTree.set(item.row, 'open', item.open ? '' : null);
            } else {
                applyEdit(showing().map(function (item) { return item.row; }), detail);
            }
        };
    )JS");
    DirectTree tree(pair.direct);
    pair.frame(2);
    // Scene is open over Camera and Lights, and Props is closed over the two rows that the script keeps under it.
    CHECK(tree.items.count() == 5);
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) == 5);
    pair.requireAlike("at rest");

    foldAndWalk(pair, tree);
    CHECK(pair.told(0) == "fold 3,1");
    CHECK(pair.told(1) == "select replace 0,1,0,0");
    CHECK(pair.told(2) == "fold 0,0");
    CHECK(pair.told(3) == "fold 0,1");
    CHECK(pair.told(4) == "select replace 1,1,1,1");
    CHECK(pair.told(5) == "select replace 2,1,2,2");
    CHECK(pair.told(6) == "fold 2,1");
    CHECK(pair.scripted.number("events.length") == 7);
    CHECK(tree.items.count() == 7);
    CHECK(tree.branches[2].selected);
    pair.requireAlike("after the folds");
}

TEST_CASE("a tree view that no script listens to opens and closes its rows itself") {
    Pair pair;
    pair.scripted.run("var attributes = {'row-height': '24', height: '220', flex: 'fixed', selectable: ''};");
    pair.scripted.run(kTreeScript);
    DirectTree tree(pair.direct);
    pair.frame(2);
    pair.requireAlike("at rest");
    foldAndWalk(pair, tree);
    CHECK(tree.items.count() == 7);
    CHECK(declaredRows(pair.scripted.fixture, pair.scriptedView()) == 7);
    pair.requireAlike("after the folds");

    // A row that closes takes the rows under it out of the view, selected or not, and a click that replaces the
    // selection clears them.
    pair.click(pair.direct.center(tree.nodes[4]));
    pair.click(pair.direct.center(tree.disclosure(3)));
    CHECK(tree.items.count() == 5);
    pair.click(pair.direct.center(tree.nodes[4]));
    pair.click(pair.direct.center(tree.disclosure(3)));
    CHECK_FALSE(tree.branches[4].selected);
    CHECK(tree.branches[6].selected);
    pair.requireAlike("after a row closed over a selected row");

    // The cursor is the view's, and a script moves it by its attributes.
    CHECK(pair.holds("kitTree.set(view, 'lead', '1') && kitTree.set(view, 'anchor', '1')"));
    tree.selection = PrKitSelection{.anchor = 1, .lead = 1};
    pair.key(PrAppKey_Down, PrAppModifierFlag_Shift);
    CHECK(tree.branches[1].selected);
    CHECK(tree.branches[2].selected);
    CHECK_FALSE(tree.branches[6].selected);
    pair.requireAlike("after a range from the cursor that the script set");
    CHECK(pair.scripted.number("events.length") == 0);
}

TEST_CASE("a visibility toggle is revealed under the pointer or by its script, and tells its click") {
    Pair pair;
    pair.scripted.run("var attributes = {'row-height': '24', height: '220', flex: 'fixed'};");
    pair.scripted.run(kTreeScript);
    pair.scripted.run(R"JS(
        model.forEach(function (item) {
            if (item.toggle !== undefined)
                kitTree.listen(item.toggle, 'click', true);
        });
        kitTree.listen(model[1].row, 'click', true);
        heard = function (node, type) {
            if (node === model[1].row)
                return;
            var item = model.find(function (item) { return item.toggle === node; });
            if (item === undefined || type !== 'click')
                throw new Error('an event of another node');
            item.hidden = !item.hidden;
            kitTree.set(item.toggle, 'hidden', item.hidden ? '' : null);
            kitTree.set(item.label, 'tone', item.hidden ? 'faint' : 'primary');
        };
    )JS");
    DirectTree tree(pair.direct);
    tree.bound = false;
    pair.frame(2);
    pair.requireAlike("with the toggle that the script reveals");
    pair.move(pair.direct.center(tree.nodes[0]));
    pair.requireAlike("with the toggle of the row under the pointer");

    pair.click(pair.direct.center(tree.toggles[0]));
    CHECK(tree.branches[0].hidden);
    CHECK(pair.scripted.number("events.length") == 1);
    CHECK(pair.holds("events[0].type === 'click' && events[0].node === model[0].toggle"));
    pair.move(PrVec2{790, 590});
    pair.requireAlike("with a hidden row, which shows its toggle wherever the pointer is");

    // A click on a row is the row's own event, and a click on a toggle is not a click on its row.
    pair.click(pair.direct.center(tree.nodes[1]));
    CHECK(pair.told(1) == "click 1");
    CHECK(pair.holds("events[1].node === model[1].row"));
    pair.click(pair.direct.center(tree.toggles[3]));
    pair.click(pair.direct.center(tree.toggles[0]));
    CHECK(tree.branches[3].hidden);
    CHECK_FALSE(tree.branches[0].hidden);
    CHECK(pair.scripted.number("events.length") == 4);
    pair.move(PrVec2{790, 590});
    pair.requireAlike("after more clicks");
}

TEST_CASE("a row dragged onto another tells where it lands, and the script moves it there") {
    struct Case {
        float at;                   // the fraction of the target row's height that the pointer is released at
        const char* zone;
        uint32_t depth;             // the depth that the dragged row lands at
        uint32_t place;             // its place among the branches once it has moved
    };
    // Camera is dragged onto Sky: before it, into it, and after it.
    const Case cases[] = {{0.15f, "before", 0, 5}, {0.5f, "inside", 1, 6}, {0.85f, "after", 0, 6}};
    for (const Case& c : cases) {
        INFO(String(PrStringView(c.zone)));
        Pair pair;
        pair.scripted.run(R"JS(
            var attributes = {'row-height': '24', height: '220', flex: 'fixed', selectable: '', 'drag-kind': '7'};
        )JS");
        pair.scripted.run(kTreeScript);
        pair.scripted.run(R"JS(
            var moved = null;
            kitTree.listen(view, 'drop', true);
            heard = function (node, type, detail) {
                if (node !== view || type !== 'drop')
                    throw new Error('an event of another node');
                var rows = showing();
                var dragged = rows[detail.values[1]];
                moved = dragged;
                var target = rows[detail.values[0]];
                model.splice(model.indexOf(dragged), 1);
                var at = model.indexOf(target) + (detail.text === 'before' ? 0 : 1);
                model.splice(at, 0, dragged);
                dragged.depth = target.depth + (detail.text === 'inside' ? 1 : 0);
                kitTree.set(dragged.row, 'depth', String(dragged.depth));
                kitTree.insert(view, dragged.row, at + 1 < model.length ? model[at + 1].row : 0);
                if (detail.text === 'inside') {
                    target.open = true;
                    kitTree.set(target.row, 'open', '');
                }
            };
        )JS");
        DirectTree tree(pair.direct);
        tree.dragKind = kRowKind;
        pair.frame(2);
        pair.requireAlike("at rest");

        PrRect box = pair.direct.bounds(tree.nodes[4]);
        PrVec2 over{pair.direct.center(tree.nodes[4]).x, box.top + (box.bottom - box.top) * c.at};
        PrVec2 from = pair.direct.center(tree.nodes[1]);
        pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
        pair.frame();
        pair.move(over);
        pair.frame();
        REQUIRE(prUiDragging(pair.scripted.fixture.ui) != nullptr);
        // Both mark the target where the drop would land.
        CHECK(jsKitRendersAlike(pair.scripted.fixture, pair.direct));
        pair.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = over}});
        pair.frame();

        // The script hears the row dropped on, the dragged row, and the dragged row's node.
        REQUIRE(tree.drops == 1);
        CHECK(tree.drop.row == 4);
        CHECK(tree.drop.item == 1);
        CHECK(pair.scripted.number("events.length") == 1);
        CHECK(pair.holds("events[0].type === 'drop'"));
        CHECK(pair.text("events[0].detail.text") == c.zone);
        CHECK(pair.holds("events[0].detail.values[0] === 4 && events[0].detail.values[1] === 1"));
        CHECK(pair.holds("moved.name === 'Camera' && events[0].detail.values[2] === moved.row"));
        Branch camera = tree.branches[1];
        camera.depth = c.depth;
        tree.branches.erase(1);
        tree.branches.insert(c.place, camera);
        if (c.depth == 1)
            tree.branches[c.place - 1].open = true;
        pair.requireAlike("after the move");
    }
}

TEST_CASE("a drop zone tells where a dragged row lands on the widget that it is under") {
    Pair pair;
    pair.scripted.run(R"JS(
        var view = make('tree-view', 0, {'drag-kind': '7', 'row-height': '24', height: '110', flex: 'fixed'});
        var rows = ['Camera', 'Sun', 'Crate'].map(function (name) {
            var row = make('tree-row', view, {});
            make('row-label', row, {}, name);
            return row;
        });
        var bin = make('box', 0, {padding: '8', height: '90', flex: 'fixed'});
        make('label', bin, {}, 'Bin');
        var zone = make('drop-zone', bin, {'drag-kind': '7'});
        kitTree.listen(zone, 'drop', true);
        kitTree.listen(zone, 'dragover', true);
    )JS");
    KitFixture& direct = pair.direct;
    const char* names[] = {"Camera", "Sun", "Crate"};
    const PrKitTreeItem items[3] = {};
    PrLayoutNode nodes[3] = {};
    PrLayoutNode bin = kPrLayoutNodeNone;
    PrKitDropZone over = PrKitDropZone_None;
    PrKitDropZone landed = PrKitDropZone_None;
    uint64_t value = 99;
    direct.build = [&](PrLayoutNode root) {
        PrKitTreeView tree{.count = 3, .items = items, .dragKind = kRowKind};
        PrKitListViewStyle style{
            .rowHeight = kTreeRowHeight,
            .place = {.height = prKitPx(110), .flex = PrKitFlex_Fixed},
        };
        PrLayoutNode view = prKitTreeView(direct.kit, root, tree, style);
        PrKitWindow window = prKitListWindow(direct.kit, view);
        for (uint32_t index = window.first; index < window.first + window.count; ++index) {
            nodes[index] = prKitTreeRow(direct.kit, view, {.index = index, .item = index});
            prKitRowLabel(direct.kit, nodes[index], {.text = names[index]}, PrKitTone_Auto);
        }
        PrKitBoxStyle box{.padding = prKitEdgesAll(8), .place = {.height = prKitPx(90), .flex = PrKitFlex_Fixed}};
        bin = prKitBox(direct.kit, root, {}, box);
        prKitLabel(direct.kit, bin, {.text = "Bin"}, {});
        over = PrKitDropZone_None;
        if (prUiIsDropTarget(direct.ui, bin, kRowKind))
            over = prKitDropZone(direct.kit, bin);
        const PrUiPayload* dropped = prUiDropped(direct.ui, bin);
        if (dropped != nullptr) {
            landed = prKitDropZone(direct.kit, bin);
            value = dropped->value;
        }
    };
    pair.frame(2);
    pair.requireAlike("at rest");

    PrRect box = direct.bounds(bin);
    PrVec2 top{direct.center(bin).x, box.top + (box.bottom - box.top) * 0.15f};
    PrVec2 bottom{top.x, box.top + (box.bottom - box.top) * 0.85f};
    PrVec2 from = direct.center(nodes[1]);
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
    pair.frame();
    pair.move(top);
    pair.frame();
    REQUIRE(prUiDragging(pair.scripted.fixture.ui) != nullptr);
    CHECK(over == PrKitDropZone_Before);
    CHECK(pair.told(0) == "dragover before");
    pair.move(bottom);
    pair.frame();
    CHECK(over == PrKitDropZone_After);
    CHECK(pair.told(1) == "dragover after");
    pair.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = bottom}});
    pair.frame();
    CHECK(landed == PrKitDropZone_After);
    CHECK(value == 1);
    // The payload's value is the dragged row's node.
    CHECK(pair.text("events[2].type + ' ' + events[2].detail.text") == "drop after");
    CHECK(pair.holds("events[2].detail.value === rows[1]"));
    CHECK(pair.told(3) == "dragover none");
    CHECK(pair.scripted.number("events.length") == 4);
    pair.requireAlike("after the drop");
}

TEST_CASE("a table's columns, rows and cells are declared as the kit's are, and its rows select") {
    Pair pair;
    pair.scripted.run(kTableScript);
    pair.scripted.run(R"JS(
        kitTree.listen(assets[4], 'click', true);
        kitTree.listen(table, 'select', true);
        heard = function (node, type, detail) {
            if (type === 'select')
                applyEdit(assets, detail);
        };
    )JS");
    DirectTable table(pair.direct);
    pair.frame(2);
    pair.requireAlike("at rest");
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // The row tells its click before the table tells the edit that the click made.
    pair.click(table.at(2));
    CHECK(pair.told(0) == "select replace 2,1,2,2");
    CHECK_FALSE(table.selected[6]);
    pair.click(table.at(4), PrAppModifierFlag_Shift);
    CHECK(pair.told(1) == "click 4");
    CHECK(pair.holds("events[1].node === assets[4]"));
    CHECK(pair.told(2) == "select replace 2,3,4,2");
    CHECK(table.selected[3]);
    pair.requireAlike("after the clicks");

    // The cursor is the table's, and a script moves it by its attributes. A key scrolls the lead into view.
    CHECK(pair.holds("kitTree.set(table, 'lead', '10') && kitTree.set(table, 'anchor', '10')"));
    table.selection = PrKitSelection{.anchor = 10, .lead = 10};
    pair.key(PrAppKey_Down, PrAppModifierFlag_Shift);
    CHECK(pair.told(3) == "select replace 10,2,11,10");
    CHECK(table.selected[11]);
    pair.requireAlike("after a range from the cursor that the script set");
    pair.key(PrAppKey_End);
    CHECK(pair.told(4) == "select replace 29,1,29,29");
    CHECK(table.selected[29]);
    CHECK(pair.scripted.number("events.length") == 5);
    pair.requireAlike("at the last row");
}

TEST_CASE("a click on a sortable heading tells its sort, and the script orders the rows") {
    Pair pair;
    pair.scripted.run(kTableScript);
    pair.scripted.run(R"JS(
        kitTree.listen(table, 'sort', true);
        heard = function (node, type, detail) {
            if (node !== table || type !== 'sort')
                throw new Error('an event of another node');
            var descending = detail.values[0] === 0 && detail.values[1] === 1;
            for (var i = 0; i < assets.length; i++)
                kitTree.insert(table, assets[descending ? assets.length - 1 - i : i], 0);
        };
    )JS");
    DirectTable table(pair.direct);
    pair.frame(2);
    pair.click(table.at(2));
    pair.requireAlike("sorted as the script's attributes say");

    // A heading that is not sortable sorts nothing. A sortable one sorts by its column, and a second click reverses the
    // order.
    pair.click(table.heading(2));
    CHECK(pair.scripted.number("events.length") == 0);
    pair.click(table.heading(0));
    CHECK(pair.told(0) == "sort 0,0");
    pair.requireAlike("sorted by the first column");
    pair.click(table.heading(0));
    CHECK(pair.told(1) == "sort 0,1");
    CHECK(table.sorts == 2);
    CHECK(table.order[0] == 29);
    pair.requireAlike("in descending order");

    // The selection is the rows' own, so the selected row is where the script moved it, among the last rows.
    PrVec2 at = table.at(4);
    pair.move(at);
    pair.push(PrAppEvent{
        .kind = PrAppEventKind_Scroll,
        .scroll = {.position = at, .delta = PrVec2{0, 30 * kTableRowHeight}},
    });
    pair.frame();
    CHECK(table.window.first + table.window.count == DirectTable::kAssets);
    CHECK(table.order[27] == 2);
    CHECK(table.selected[2]);
    pair.requireAlike("at the last rows");

    // The script's attributes overwrite the sort that a click wrote.
    CHECK(pair.holds("kitTree.set(table, 'sort-column', null) && kitTree.set(table, 'sort-descending', null)"));
    table.sort = PrKitTableSort{.column = kPrKitColumnNone};
    pair.requireAlike("sorted by no column");
}

TEST_CASE("a row, a column or a cell outside its own view, table or row declares nothing") {
    Pair pair;
    pair.scripted.run(R"JS(
        var page = make('box', 0, {gap: '4'});
        make('list-row', page, {selected: ''});
        make('list-tile', page, {});
        make('tree-row', page, {depth: '2'});
        make('table-row', page, {});
        make('table-column', page, {width: '80'}, 'LOST');
        make('table-cell', page, {}, 'lost');

        // The parts of a row and a tile are widgets wherever they are.
        var line = make('box', page, {direction: 'row', align: 'center', gap: '4'});
        make('row-icon', line, {icon: 'cube'});
        make('row-label', line, {tone: 'accent'}, 'A label on its own');
        make('row-meta', line, {}, 'meta');
        make('visibility-toggle', line, {hidden: ''});
        make('tile-label', page, {}, 'A tile label on its own');

        // A view declares the rows of its own kind, and what else is under it after them.
        var list = make('list-view', page, {height: '60', flex: 'fixed'});
        make('tree-row', list, {});
        make('table-row', list, {});
        make('label', list, {}, 'stray');
        var row = make('list-row', list, {});
        make('row-label', row, {}, 'The one row');

        // A cell takes its column from its place, and a cell past the table's columns is left out.
        var table = make('table', page, {height: '80', flex: 'fixed'});
        make('table-column', table, {width: '90'}, '');
        var cells = make('table-row', table, {});
        make('table-cell', cells, {}, 'kept');
        make('table-cell', cells, {}, 'past the columns');
        make('table-cell', cells, {column: '7'}, 'no such column');
        make('list-row', table, {});

        var bare = make('table', page, {height: '60', flex: 'fixed'});
        make('table-cell', make('table-row', bare, {}), {}, 'no columns');
        make('tree-view', page, {height: '40', flex: 'fixed', selectable: '', 'drag-kind': '3'});
    )JS");
    KitFixture& direct = pair.direct;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode page = prKitBox(kit, root, {}, {.gap = 4});
        PrKitBoxStyle row{.direction = PrKitDirection_Row, .align = PrKitAlign_Center, .gap = 4};
        PrLayoutNode line = prKitBox(kit, page, {}, row);
        prKitRowIcon(kit, line, {.icon = PrKitIconKind_Cube});
        prKitRowLabel(kit, line, {.text = "A label on its own"}, PrKitTone_Accent);
        prKitRowMeta(kit, line, {.text = "meta"});
        prKitVisibilityToggle(kit, line, {.hidden = true});
        prKitTileLabel(kit, page, {.text = "A tile label on its own"});

        PrLayoutNode list =
            prKitListView(kit, page, {.count = 1}, {.place = {.height = prKitPx(60), .flex = PrKitFlex_Fixed}});
        if (prKitListWindow(kit, list).count == 1)
            prKitRowLabel(kit, prKitListRow(kit, list, {.index = 0, .item = 0}), {.text = "The one row"},
                PrKitTone_Auto);
        prKitLabel(kit, list, {.text = "stray"}, {});

        // The script's tables are bound to a sort, as every table of the binding is.
        const PrKitTableColumn column{.label = "", .width = 90};
        PrKitTableSort sort{.column = kPrKitColumnNone};
        PrKitTable one{.count = 1, .columns = &column, .columnCount = 1, .sort = &sort};
        PrLayoutNode table = prKitTable(kit, page, one, {.place = {.height = prKitPx(80), .flex = PrKitFlex_Fixed}});
        if (prKitListWindow(kit, table).count == 1)
            prKitTableCell(kit, prKitTableRow(kit, table, {.index = 0, .item = 0}), {.column = 0, .text = "kept"});

        PrKitTableSort unsorted{.column = kPrKitColumnNone};
        PrKitTable none{.count = 1, .columns = &column, .columnCount = 0, .sort = &unsorted};
        PrLayoutNode bare = prKitTable(kit, page, none, {.place = {.height = prKitPx(60), .flex = PrKitFlex_Fixed}});
        if (prKitListWindow(kit, bare).count == 1)
            prKitTableRow(kit, bare, {.index = 0, .item = 0});

        PrKitSelection selection{};
        PrKitTreeView tree{.selection = &selection, .dragKind = 3};
        prKitTreeView(kit, page, tree, {.place = {.height = prKitPx(40), .flex = PrKitFlex_Fixed}});
    };
    pair.frame(2);
    pair.requireAlike("at rest");
}

TEST_CASE("an attribute of a collection takes only the values of its kind") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var kinds = [
            'list-view', 'list-row', 'list-tile', 'row-icon', 'row-label', 'row-meta', 'tile-icon', 'tile-image',
            'tile-label', 'empty-state', 'tree-view', 'tree-row', 'visibility-toggle', 'drop-zone', 'table',
            'table-column', 'table-row', 'table-cell',
        ];
        var node = {};
        kinds.forEach(function (kind) { node[kind] = kitTree.create(kind); });
        function set(kind, name, value) { return kitTree.set(node[kind], name, value); }
        function listen(kind, type) { return kitTree.listen(node[kind], type, true); }
        var taken = [
            set('list-view', 'row-height', '30'), set('list-view', 'row-height', null),
            set('list-view', 'tile-width', '96'), set('list-view', 'selectable', ''), set('list-view', 'anchor', '3'),
            set('list-view', 'lead', '4'), set('list-view', 'max-height', '50%'),
            set('list-row', 'selected', 'true'), set('list-tile', 'selected', 'false'),
            set('row-icon', 'icon', 'cube'), set('row-label', 'tone', 'faint'),
            set('tile-icon', 'icon', 'sphere'), set('tile-icon', 'tint', 'accent'), set('tile-icon', 'tint', '#ff0000'),
            set('tile-icon', 'tint', '0.27 0.4 0.33'), set('tile-icon', 'tint', null),
            set('tile-image', 'hover-zoom', '0.25'), set('tile-image', 'image', '1 1 /wAA/w=='),
            set('tile-image', 'image', null),
            set('empty-state', 'icon', 'search'), set('empty-state', 'hint', 'A hint'),
            set('tree-view', 'row-height', '18'), set('tree-view', 'drag-kind', '7'), set('tree-view', 'lead', '2'),
            set('tree-row', 'depth', '65535'), set('tree-row', 'open', ''), set('tree-row', 'has-children', ''),
            set('tree-row', 'selected', ''),
            set('visibility-toggle', 'hidden', ''), set('visibility-toggle', 'revealed', 'hover'),
            set('visibility-toggle', 'revealed', ''), set('visibility-toggle', 'revealed', 'false'),
            set('visibility-toggle', 'revealed', null),
            set('drop-zone', 'drag-kind', '7'),
            set('table', 'sort-column', '2'), set('table', 'sort-column', null), set('table', 'sort-descending', ''),
            set('table', 'selectable', ''), set('table', 'row-height', '24'),
            set('table-column', 'align', 'center'), set('table-column', 'sortable', ''),
            set('table-column', 'width', '120'), set('table-column', 'flex', 'fill'),
            set('table-column', 'weight', '2'),
            set('table-row', 'selected', ''),
            set('table-cell', 'column', '1'), set('table-cell', 'column', null), set('table-cell', 'icon', 'cube'),
            set('table-cell', 'role', 'mono'), set('table-cell', 'tone', 'muted'),
            listen('list-view', 'select'), listen('tree-view', 'select'), listen('tree-view', 'fold'),
            listen('tree-view', 'drop'), listen('table', 'select'), listen('table', 'sort'),
            listen('list-row', 'click'), listen('list-tile', 'click'), listen('tree-row', 'click'),
            listen('table-row', 'click'), listen('visibility-toggle', 'click'), listen('drop-zone', 'drop'),
            listen('drop-zone', 'dragover'),
        ];
        var refused = [
            set('list-view', 'row-height', '-1'), set('list-view', 'row-height', '0.5'),
            set('list-view', 'tile-width', 'wide'), set('list-view', 'tile-width', '0.25'),
            set('list-view', 'selectable', 'yes'), set('list-view', 'lead', '-1'), set('list-view', 'anchor', '1.5'),
            set('list-view', 'drag-kind', '7'), set('list-view', 'sort-column', '0'),
            set('list-row', 'selected', 'yes'), set('list-row', 'depth', '1'), set('list-row', 'open', ''),
            set('list-tile', 'has-children', ''),
            set('row-icon', 'icon', 'no-such-icon'), set('row-label', 'tone', 'loud'),
            set('row-meta', 'tone', 'muted'), set('tile-label', 'icon', 'cube'),
            set('tile-icon', 'tint', '0.27 0.4'), set('tile-icon', 'tint', 'no-such-color'),
            set('tile-image', 'hover-zoom', '-1'), set('tile-image', 'image', ''),
            set('tile-image', 'image', '2 2 /wAA/w=='), set('tile-image', 'image', '0 1 '),
            set('tile-image', 'image', '5000 1 AAAA'), set('tile-image', 'image', '1 1 !!!!!!=='),
            set('tile-image', 'image', '1 /wAA/w=='), set('tile-image', 'image', 'fox.png'),
            set('empty-state', 'icon', 'no-such-icon'), set('empty-state', 'title', 'A title'),
            set('tree-view', 'tile-width', '100'), set('tree-view', 'drag-kind', 'rows'),
            set('tree-view', 'sort-descending', ''),
            set('tree-row', 'depth', '-1'), set('tree-row', 'depth', '65536'), set('tree-row', 'depth', 'deep'),
            set('tree-row', 'open', 'maybe'), set('tree-row', 'has-children', '2'),
            set('visibility-toggle', 'revealed', 'sometimes'), set('visibility-toggle', 'hidden', 'hover'),
            set('drop-zone', 'drag-kind', '-7'), set('drop-zone', 'zone', 'before'),
            set('table', 'tile-width', '100'), set('table', 'sort-column', 'name'), set('table', 'sort-column', '-1'),
            set('table', 'sort-descending', 'down'), set('table', 'drag-kind', '7'),
            set('table-column', 'align', 'justify'), set('table-column', 'sortable', '2'),
            set('table-column', 'label', 'NAME'),
            set('table-row', 'depth', '1'),
            set('table-cell', 'column', 'first'), set('table-cell', 'column', '-1'), set('table-cell', 'role', 'huge'),
            set('table-cell', 'tone', 'loud'), set('table-cell', 'icon', 'no-such-icon'),
            listen('list-view', 'fold'), listen('list-view', 'sort'), listen('tree-view', 'sort'),
            listen('table', 'drop'), listen('table', 'fold'), listen('list-row', 'select'),
            listen('table-cell', 'click'), listen('table-column', 'click'), listen('row-label', 'click'),
            listen('drop-zone', 'dragenter'), listen('empty-state', 'click'),
        ];
        var takenCount = taken.filter(Boolean).length;
        var refusedCount = refused.filter(Boolean).length;
    )JS");
    CHECK(scripted.number("takenCount") == scripted.number("taken.length"));
    CHECK(scripted.number("refusedCount") == 0);
}
