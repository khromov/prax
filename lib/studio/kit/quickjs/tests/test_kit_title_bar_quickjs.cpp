// The kinds of Studio Kit's title bar in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/app_window.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_text.h"
#include "pr/kit_title_bar.h"
#include "pr/mem.h"
#include "pr/ui.h"

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

// Returns true if the last frames of both kits marked the same title bar regions, and writes their number to `count`.
bool sameRegions(KitFixture& first, KitFixture& second, uint32_t* count) {
    PrAppOutput a = prUiOutput(first.ui);
    PrAppOutput b = prUiOutput(second.ui);
    *count = a.titleBarRegionCount;
    if (a.titleBarRegionCount != b.titleBarRegionCount)
        return false;
    for (uint32_t i = 0; i < a.titleBarRegionCount; ++i) {
        const PrAppTitleBarRegion& one = a.titleBarRegions[i];
        const PrAppTitleBarRegion& other = b.titleBarRegions[i];
        if (one.kind != other.kind || !prMemEquals(&one.rect, &other.rect, sizeof one.rect))
            return false;
    }
    return true;
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a title bar is the band that a script describes, and holds the nodes under it in its row") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var clicks = 0;
        var bar = make('title-bar', 0, {
            'bar-height': '32', 'bar-leading': '70', 'bar-trailing': '8', 'program-buttons': '', maximized: '',
            margin: '0 0 0 12',
        });
        var menu = make('button', bar, {variant: 'ghost', size: 'sm'}, 'File');
        make('label', bar, {tone: 'secondary'}, 'Untitled');
        make('label', 0, {}, 'The window under its title bar');
        kitTree.listen(menu, 'click', true);
        kitTree.onevent = function (node, type) { clicks += node === menu && type === 'click' ? 1 : 0; };
    )JS");
    KitFixture direct;
    PrKitTitleBar titleBar{
        .bar = {.height = 32, .leading = 70, .trailing = 8, .flags = PrAppTitleBarFlag_ProgramButtons},
        .maximized = true,
    };
    PrLayoutNode menu = kPrLayoutNodeNone;
    uint32_t clicks = 0;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitTitleBar(kit, root, titleBar);
        prKitPlace(kit, row, {.margin = PrKitEdges{0, 0, 0, 12}});
        menu =
            prKitButton(kit, row, {.text = "File"}, {.variant = PrKitButtonVariant_Ghost, .size = PrKitControlSize_Sm});
        clicks += prKitActivated(kit, menu) ? 1 : 0;
        prKitLabel(kit, row, {.text = "Untitled"}, {.tone = PrKitTone_Secondary});
        prKitLabel(kit, root, {.text = "The window under its title bar"}, {});
    };
    // A region is marked from the layout of the frame before, so a second frame marks them all.
    frames(scripted, direct, 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    uint32_t regions = 0;
    CHECK(sameRegions(scripted.fixture, direct, &regions));
    CHECK(regions == 5);

    // A control in the row takes a press, which the script hears of as it does anywhere else.
    PrVec2 at = direct.center(menu);
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(clicks == 1);
    CHECK(scripted.number("clicks") == 1);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A window that is not maximized, whose buttons the system draws in the trailing inset.
    scripted.run(R"JS(
        kitTree.set(bar, 'maximized', null);
        kitTree.set(bar, 'program-buttons', null);
        kitTree.set(bar, 'bar-leading', null);
        kitTree.set(bar, 'bar-trailing', '138');
        kitTree.set(bar, 'bar-height', '28');
    )JS");
    titleBar = PrKitTitleBar{.bar = {.height = 28, .trailing = 138}};
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(sameRegions(scripted.fixture, direct, &regions));
    CHECK(regions == 2);

    // Reset, the bar's height is that of a window without a custom title bar: a plain row, which drags nothing.
    scripted.run("kitTree.set(bar, 'bar-height', null); kitTree.set(bar, 'bar-trailing', null);");
    titleBar = PrKitTitleBar{};
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(sameRegions(scripted.fixture, direct, &regions));
    CHECK(regions == 0);
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a title bar takes only its own attributes, and no events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var bar = kitTree.create('title-bar');
        var taken = [
            kitTree.set(bar, 'bar-height', '30.5'), kitTree.set(bar, 'bar-leading', '0'),
            kitTree.set(bar, 'bar-trailing', '12'), kitTree.set(bar, 'program-buttons', 'true'),
            kitTree.set(bar, 'program-buttons', 'false'), kitTree.set(bar, 'maximized', ''),
            kitTree.set(bar, 'height', '40'),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(bar, 'bar-height', '-1'), kitTree.set(bar, 'bar-height', 'auto'),
            kitTree.set(bar, 'bar-leading', '10%'), kitTree.set(bar, 'bar-trailing', ''),
            kitTree.set(bar, 'program-buttons', 'yes'), kitTree.set(bar, 'maximized', '1'),
            kitTree.set(bar, 'bar', '32 70 8'), kitTree.set(bar, 'leading', '70'), kitTree.set(bar, 'flags', '1'),
            kitTree.set(bar, 'title', 'Untitled'), kitTree.listen(bar, 'click', true),
            kitTree.listen(bar, 'close', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 7);
    CHECK(scripted.number("refused") == 0);
}
