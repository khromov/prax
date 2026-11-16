// The kinds of Studio Kit's icons in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/color.h"
#include "pr/kit_box.h"
#include "pr/kit_icon.h"
#include "pr/kit_text.h"
#include "pr/path.h"
#include "pr/path_data.h"
#include "pr/status.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// Runs `count` frames of both kits.
void frames(JsKitScripted& scripted, KitFixture& direct, uint32_t count = 1) {
    for (uint32_t i = 0; i < count; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
}

// Returns a path of `data`, which the caller destroys.
PrPath* pathFrom(PrStringView data) {
    PrPath* path = prPathCreate();
    REQUIRE(prPathAppendData(path, data) == PrStatus_Ok);
    return path;
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("an icon draws the icon of the set that a script names, at its size and in its tone") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '8', padding: '8'});
        make('icon', row, {icon: 'search'});
        var large = make('icon', row, {icon: 'folder-open', size: '32', tone: 'accent', margin: '0 12'});
        make('icon', row, {icon: 'star', size: '12', tone: 'warning', 'align-self': 'end'});
        make('icon', row, {size: '24'});
        make('label', large, {}, 'An icon holds nothing, so this is not declared');
        make('label', row, {}, 'after the icons');
    )JS");
    KitFixture direct;
    PrKitIconKind shown = PrKitIconKind_FolderOpen;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        });
        prKitIcon(kit, row, {.icon = PrKitIconKind_Search}, {});
        PrLayoutNode large = prKitIcon(kit, row, {.icon = shown}, {.size = 32, .tone = PrKitTone_Accent});
        prKitPlace(kit, large, {.margin = prKitEdgesXY(0, 12)});
        PrLayoutNode star = prKitIcon(kit, row, {.icon = PrKitIconKind_Star}, {.size = 12, .tone = PrKitTone_Warning});
        prKitPlace(kit, star, {.align = PrKitAlign_End});
        prKitIcon(kit, row, {}, {.size = 24});
        prKitLabel(kit, row, {.text = "after the icons"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // An icon that the set does not have is refused, and the node keeps the one that it had.
    scripted.run("var refused = kitTree.set(large, 'icon', 'no-such-icon');");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(large, 'icon', 'window-restore');");
    shown = PrKitIconKind_WindowRestore;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(large, 'icon', null);");
    shown = PrKitIconKind_None;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("an icon, which holds nothing, tells where it was laid out and is brought into view as any widget is") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var column = make('box', 0, {overflow: 'scroll-y', height: '120', gap: '8'});
        var icons = [];
        for (var i = 0; i < 20; ++i)
            icons.push(make('icon', column, {icon: 'star', size: '24'}));
        make('label', icons[19], {}, 'not declared');
        var told = null;
        kitTree.listen(icons[19], 'bounds', true);
        kitTree.onevent = function (node, type, detail) {
            if (node !== icons[19] || type !== 'bounds')
                throw new Error('an event of another node');
            told = detail.values;
        };
    )JS");
    KitFixture direct;
    PrLayoutNode last = kPrLayoutNodeNone;
    bool reveal = false;
    direct.build = [&](PrLayoutNode root) {
        PrKitBoxStyle style{.overflow = PrKitOverflow_ScrollY, .gap = 8};
        style.place.height = prKitPx(120);
        PrLayoutNode column = prKitBox(direct.kit, root, {}, style);
        for (uint32_t i = 0; i < 20; ++i)
            last = prKitIcon(direct.kit, column, {.icon = PrKitIconKind_Star}, {.size = 24});
        if (reveal)
            prUiReveal(direct.kit->ui, last);
        reveal = false;
    };
    frames(scripted, direct, 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    PrRect box = direct.bounds(last);
    CHECK(box.top > 120);
    CHECK(scripted.number("told[1]") == doctest::Approx(double(box.top)));
    CHECK(scripted.number("told[3]") == doctest::Approx(double(box.bottom)));

    scripted.run("kitTree.set(icons[19], 'reveal', '1');");
    reveal = true;
    frames(scripted, direct, 30);
    box = direct.bounds(last);
    CHECK(box.bottom <= 120.5f);
    CHECK(scripted.number("told[3]") == doctest::Approx(double(box.bottom)));
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a path icon draws the path data that a script gives, stroked or filled in its color, and follows its edits") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '8', padding: '8'});
        var cross = make('path-icon', row, {path: 'M4 12 L20 12 M12 4 L12 20', color: 'danger', stroked: '', size: '32'});
        var flag = make('path-icon', row, {path: 'M4 4 H20 L14 10 L20 16 H4 Z', color: '#33ac5a', margin: '0 0 0 12'});
        make('path-icon', row, {size: '24'});
        make('label', cross, {}, 'A path icon holds nothing, so this is not declared');
        make('label', row, {}, 'after the icons');
    )JS");
    KitFixture direct;
    PrPath* cross = pathFrom("M4 12 L20 12 M12 4 L12 20");
    PrPath* ring = pathFrom("M12 4 A8 8 0 1 0 12 20 A8 8 0 1 0 12 4 Z");
    PrPath* flag = pathFrom("M4 4 H20 L14 10 L20 16 H4 Z");
    PrPath* empty = prPathCreate();
    const PrPath* first = cross;
    PrColor firstColor{};
    bool themed = true;
    bool stroked = true;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        });
        PrColor color = themed ? kit->theme.palette.danger : firstColor;
        prKitPathIcon(kit, row, {.path = first, .color = color, .stroked = stroked}, {.size = 32});
        PrColor green = prColorFromColor8(PrColor8{0x33, 0xac, 0x5a, 0xff});
        PrLayoutNode filled = prKitPathIcon(kit, row, {.path = flag, .color = green}, {});
        prKitPlace(kit, filled, {.margin = PrKitEdges{0, 0, 0, 12}});
        prKitPathIcon(kit, row, {.path = empty}, {.size = 24});
        prKitLabel(kit, row, {.text = "after the icons"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // Path data in error is refused whole, and the icon keeps its path.
    scripted.run("var refused = kitTree.set(cross, 'path', 'M4 12 L20 12 L');");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The icon is drawn again for another path, another color, and filled where it was stroked.
    scripted.run("kitTree.set(cross, 'path', 'M12 4 A8 8 0 1 0 12 20 A8 8 0 1 0 12 4 Z');");
    first = ring;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(cross, 'color', '0.9 0.8 0.1 0.75');");
    themed = false;
    firstColor = PrColor{0.9f, 0.8f, 0.1f, 0.75f};
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(cross, 'stroked', null);");
    stroked = false;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // Reset, the path is empty.
    scripted.run("kitTree.set(cross, 'path', null);");
    first = empty;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    prPathDestroy(cross);
    prPathDestroy(ring);
    prPathDestroy(flag);
    prPathDestroy(empty);
}

TEST_CASE("a path icon's widget draws its path until a round declares it no more, whatever the script does to its node") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var kept = make('path-icon', 0, {path: 'M4 12 L20 12', color: 'accent', stroked: '', size: '48'});
        var gone = make('path-icon', 0, {path: 'M12 4 L12 20', color: 'accent', stroked: '', size: '48'});
        function edit() {
            kitTree.set(kept, 'path', 'M4 4 L20 20');
            kitTree.destroy(gone);
        }
    )JS");
    // The script replaces one icon's path and destroys the other icon right after the round that first declares them,
    // as a listener may that hears of an event of that round, and so before anything has painted them.
    bool edited = false;
    scripted.fixture.build = [&](PrLayoutNode root) {
        prJsKitTreeBuild(scripted.tree, scripted.fixture.kit, root);
        if (!edited)
            scripted.run("edit();");
        edited = true;
    };
    KitFixture direct;
    PrPath* across = pathFrom("M4 12 L20 12");
    PrPath* down = pathFrom("M12 4 L12 20");
    PrPath* diagonal = pathFrom("M4 4 L20 20");
    bool first = true;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrColor accent = kit->theme.palette.accent;
        prKitPathIcon(kit, root, {.path = first ? across : diagonal, .color = accent, .stroked = true}, {.size = 48});
        if (first)
            prKitPathIcon(kit, root, {.path = down, .color = accent, .stroked = true}, {.size = 48});
    };

    // The frame's one round declared the widgets with the paths that the nodes had then. Its tree holds them so, and
    // painting it, and recording it again to verify it, draw those paths.
    scripted.fixture.frame(1.0 / 60, 1);
    direct.frame(1.0 / 60, 1);
    CHECK(edited);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // The next round declares what the script left.
    first = false;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    prPathDestroy(across);
    prPathDestroy(down);
    prPathDestroy(diagonal);
}

TEST_CASE("the kinds of icons take only their own attributes, and no events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var icon = kitTree.create('icon');
        var path = kitTree.create('path-icon');
        var taken = [
            kitTree.set(icon, 'icon', 'chevron-down'), kitTree.set(icon, 'size', '0'), kitTree.set(icon, 'tone', 'faint'),
            kitTree.set(icon, 'width', '20'), kitTree.set(path, 'path', ''), kitTree.set(path, 'path', 'M0 0'),
            kitTree.set(path, 'color', 'text1'), kitTree.set(path, 'stroked', 'true'), kitTree.set(path, 'size', '18.5'),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(icon, 'icon', ''), kitTree.set(icon, 'icon', 'Search'), kitTree.set(icon, 'size', '-2'),
            kitTree.set(icon, 'tone', 'loud'), kitTree.set(icon, 'path', 'M0 0'), kitTree.set(icon, 'color', 'accent'),
            kitTree.set(path, 'path', 'L4 4'), kitTree.set(path, 'path', 'M0 0 L 1'), kitTree.set(path, 'path', 'path'),
            kitTree.set(path, 'color', 'red'), kitTree.set(path, 'stroked', '1'), kitTree.set(path, 'tone', 'accent'),
            kitTree.set(path, 'icon', 'search'), kitTree.listen(icon, 'click', true), kitTree.listen(path, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 9);
    CHECK(scripted.number("refused") == 0);
}
