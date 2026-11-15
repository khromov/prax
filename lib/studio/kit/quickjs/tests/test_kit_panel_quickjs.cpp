// The kinds of Studio Kit's panels in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/color.h"
#include "pr/count_of.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_panel.h"
#include "pr/kit_text.h"

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

// A keyword of an attribute, and the value of the kit's that it stands for.
struct Keyword {
    const char* name;
    uint8_t value;
};

// Sets the attribute `name` of the script's node `node`, a global of the script, to `value`.
void set(JsKitScripted& scripted, const char* node, const char* name, const char* value) {
    String source("if (!kitTree.set(");
    source.append(node).append(", '").append(name).append("', '").append(value);
    source.append("')) throw new Error('refused');").append('\0');
    scripted.run(source.data());
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a panel is a box on the background and with the rules that a script names") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var bar = make('panel', 0, {
            background: 'header', rules: 'top bottom', direction: 'row', justify: 'center', align: 'center', gap: '8',
            padding: '4 12', overflow: 'clip', wrap: '', disabled: '', height: '40', margin: '8',
        });
        make('label', bar, {}, 'A bar that disables what it holds');
        make('button', bar, {}, 'Off');
        var card = make('panel', 0, {background: 'panel', rules: 'all', padding: '12', width: '240', 'align-self': 'start'});
        make('label', card, {}, 'A card');
    )JS");
    KitFixture direct;
    PrKitBackground background = PrKitBackground_Panel;
    PrKitRuleFlags rules = PrKitRuleFlag_All;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode bar = prKitPanel(kit, root, {.disabled = true}, {
            .box = {
                .direction = PrKitDirection_Row,
                .justify = PrKitJustify_Center,
                .align = PrKitAlign_Center,
                .overflow = PrKitOverflow_Clip,
                .wrap = true,
                .gap = 8,
                .padding = prKitEdgesXY(4, 12),
                .place = {.height = prKitPx(40), .margin = prKitEdgesAll(8)},
            },
            .background = PrKitBackground_Header,
            .rules = PrKitRuleFlag_Top | PrKitRuleFlag_Bottom,
        });
        prKitLabel(kit, bar, {.text = "A bar that disables what it holds"}, {});
        prKitButton(kit, bar, {.text = "Off"}, {});
        PrLayoutNode card = prKitPanel(kit, root, {}, {
            .box = {.padding = prKitEdgesAll(12), .place = {.width = prKitPx(240), .align = PrKitAlign_Start}},
            .background = background,
            .rules = rules,
        });
        prKitLabel(kit, card, {.text = "A card"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    const Keyword backgrounds[] = {
        {"none", PrKitBackground_None}, {"app", PrKitBackground_App}, {"panel", PrKitBackground_Panel},
        {"header", PrKitBackground_Header}, {"raised", PrKitBackground_Raised}, {"select", PrKitBackground_Select},
        {"viewport", PrKitBackground_Viewport}, {"hud", PrKitBackground_Hud},
    };
    for (const Keyword& keyword : backgrounds) {
        CAPTURE(keyword.name);
        set(scripted, "card", "background", keyword.name);
        background = keyword.value;
        frames(scripted, direct);
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }
    const Keyword sides[] = {
        {"top", PrKitRuleFlag_Top}, {"right", PrKitRuleFlag_Right}, {"bottom", PrKitRuleFlag_Bottom},
        {"left", PrKitRuleFlag_Left}, {" left  top ", PrKitRuleFlag_Left | PrKitRuleFlag_Top},
        {"all", PrKitRuleFlag_All},
    };
    for (const Keyword& keyword : sides) {
        CAPTURE(keyword.name);
        set(scripted, "card", "rules", keyword.name);
        rules = keyword.value;
        frames(scripted, direct);
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }

    // Reset, a panel has no rules and no background.
    scripted.run("kitTree.set(card, 'rules', null); kitTree.set(card, 'background', null);");
    rules = 0;
    background = PrKitBackground_None;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a selectable queues a click when it is activated, and shows the selection that the script sets") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var list = make('box', 0, {width: '200', gap: '2'});
        var rows = ['Scene', 'Assets', 'Console', 'Locked'].map(function (name) {
            const row = make('selectable', list, {
                direction: 'row', align: 'center', gap: '8', padding: '4 8', width: '160', 'align-self': 'end',
            });
            make('label', row, {}, name);
            kitTree.listen(row, 'click', true);
            return row;
        });
        kitTree.set(rows[0], 'selected', '');
        kitTree.set(rows[3], 'disabled', '');
        var clicked = [];
        var details = 0;
        kitTree.onevent = function (node, type, detail) {
            if (type !== 'click')
                throw new Error('an event of another type');
            clicked.push(rows.indexOf(node));
            details += Object.keys(detail).length;
            rows.forEach(function (row) { kitTree.set(row, 'selected', row === node ? '' : null); });
        };
    )JS");
    KitFixture direct;
    PrLayoutNode rows[4] = {};
    uint32_t selected = 0;
    uint32_t clicks = 0;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode list = prKitBox(kit, root, {}, {.gap = 2, .place = {.width = prKitPx(200)}});
        const char* names[] = {"Scene", "Assets", "Console", "Locked"};
        for (uint32_t i = 0; i < 4; ++i) {
            rows[i] = prKitSelectable(kit, list, {.selected = selected == i, .disabled = i == 3}, {
                .direction = PrKitDirection_Row,
                .align = PrKitAlign_Center,
                .gap = 8,
                .padding = prKitEdgesXY(4, 8),
                .place = {.width = prKitPx(160), .align = PrKitAlign_End},
            });
            prKitLabel(kit, rows[i], {.text = names[i]}, {});
            if (prKitActivated(kit, rows[i])) {
                selected = i;
                ++clicks;
            }
        }
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A click selects the row under it in both, once the script has set what the event told it.
    PrVec2 at = direct.center(rows[2]);
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(clicks == 1);
    CHECK(selected == 2);
    CHECK(scripted.number("clicked.length") == 1);
    CHECK(scripted.number("clicked[0]") == 2);
    CHECK(scripted.number("details") == 0);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The row that was clicked has the focus, so Enter activates it again. A disabled row takes no click.
    scripted.fixture.key(PrAppKey_Enter);
    direct.key(PrAppKey_Enter);
    frames(scripted, direct);
    CHECK(clicks == 2);
    CHECK(scripted.number("clicked.length") == 2);
    at = direct.center(rows[3]);
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct, 30);
    CHECK(clicks == 2);
    CHECK(scripted.number("clicked.length") == 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a panel header shows its title and its meta, and holds the nodes under it after them") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var header = make('panel-header', 0, {meta: 'POLL 125 HZ'}, 'INPUT DEVICE');
        make('badge', header, {variant: 'success'}, 'live');
        var bare = make('panel-header', 0, {}, 'SCENE');
    )JS");
    KitFixture direct;
    const char* meta = "POLL 125 HZ";
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode header = prKitPanelHeader(kit, root, {.title = "INPUT DEVICE", .meta = meta});
        prKitBadge(kit, header, {.text = "live"}, {.variant = PrKitBadgeVariant_Success});
        prKitPanelHeader(kit, root, {.title = "SCENE"});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    scripted.run("kitTree.set(header, 'meta', '23');");
    meta = "23";
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(header, 'meta', null);");
    meta = "";
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a swatch is filled with the color that a script names, in each form that a color takes") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '4', padding: '8'});
        var colors = ['accent', '#e24947', '#0F08', '#1af', '#33ac5a80', '0.2 0.4 0.6', ' 1 0.5 0  0.5 '];
        var swatches = colors.map(function (color, index) {
            return make('swatch', row, {color: color, size: String(12 + 4 * index)});
        });
        var unset = make('swatch', row, {size: '12', margin: '0 0 0 8'});
    )JS");
    KitFixture direct;
    PrColor own = PrColor{1, 0.5f, 0, 0.5f};
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .gap = 4,
            .padding = prKitEdgesAll(8),
        });
        const PrColor colors[] = {
            kit->theme.palette.accent,
            prColorFromColor8(PrColor8{0xe2, 0x49, 0x47, 0xff}),
            prColorFromColor8(PrColor8{0x00, 0xff, 0x00, 0x88}),
            prColorFromColor8(PrColor8{0x11, 0xaa, 0xff, 0xff}),
            prColorFromColor8(PrColor8{0x33, 0xac, 0x5a, 0x80}),
            PrColor{0.2f, 0.4f, 0.6f, 1},
            own,
        };
        for (uint32_t i = 0; i < countOf(colors); ++i)
            prKitSwatch(kit, row, {.color = colors[i]}, {.size = float(12 + 4 * i)});
        PrLayoutNode unset = prKitSwatch(kit, row, {}, {.size = 12});
        prKitPlace(kit, unset, {.margin = PrKitEdges{0, 0, 0, 8}});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A color that the swatch refuses leaves it as it was, and a reset color is transparent.
    scripted.run("var refused = kitTree.set(swatches[6], 'color', '1 0.5');");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(swatches[6], 'color', null);");
    own = PrColor{};
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("the kinds of panels take only their own attributes and events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var panel = kitTree.create('panel');
        var selectable = kitTree.create('selectable');
        var header = kitTree.create('panel-header');
        var swatch = kitTree.create('swatch');
        var taken = [
            kitTree.set(panel, 'rules', 'top top'), kitTree.set(panel, 'gap', '4'), kitTree.set(panel, 'width', 'fill'),
            kitTree.set(selectable, 'overflow', 'scroll'), kitTree.set(selectable, 'selected', 'false'),
            kitTree.set(header, 'meta', ''), kitTree.set(swatch, 'color', 'text-inverse'),
            kitTree.set(swatch, 'color', '-0.5 2 0'), kitTree.set(swatch, 'size', '0'),
            kitTree.listen(selectable, 'click', true), kitTree.listen(selectable, 'click', false),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(panel, 'rules', ''), kitTree.set(panel, 'rules', 'top middle'), kitTree.set(panel, 'rules', 'TOP'),
            kitTree.set(panel, 'background', 'glass'), kitTree.set(panel, 'selected', ''),
            kitTree.set(selectable, 'background', 'panel'), kitTree.set(selectable, 'selected', 'yes'),
            kitTree.set(header, 'title', 'SCENE'), kitTree.set(header, 'gap', '4'),
            kitTree.set(swatch, 'color', 'nope'), kitTree.set(swatch, 'color', ''), kitTree.set(swatch, 'color', '#'),
            kitTree.set(swatch, 'color', '#12'), kitTree.set(swatch, 'color', '#12345'),
            kitTree.set(swatch, 'color', '#ggg'), kitTree.set(swatch, 'color', '#1234567'),
            kitTree.set(swatch, 'color', '1 2'), kitTree.set(swatch, 'color', '1 2 3 4 5'),
            kitTree.set(swatch, 'color', '1 2 x'), kitTree.set(swatch, 'color', 'accent 1'),
            kitTree.set(swatch, 'size', '-1'), kitTree.set(swatch, 'size', 'md'),
            kitTree.listen(panel, 'click', true), kitTree.listen(selectable, 'change', true),
            kitTree.listen(header, 'click', true), kitTree.listen(swatch, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 11);
    CHECK(scripted.number("refused") == 0);
}
