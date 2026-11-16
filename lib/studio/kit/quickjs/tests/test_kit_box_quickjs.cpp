// The kinds of Studio Kit's boxes in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/geom.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_choice.h"
#include "pr/kit_input.h"
#include "pr/kit_panel.h"
#include "pr/kit_text.h"
#include "pr/string.h"
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

// Defines the script's global `name` as the terms of `matrix`, numbers in the order that a plane's `transform` takes
// them.
void defineMatrix(JsKitScripted& scripted, const char* name, const PrMatrix4& matrix) {
    JSContext* ctx = scripted.ctx;
    JSValue terms = JS_NewArray(ctx);
    for (uint32_t i = 0; i < 16; ++i)
        JS_SetPropertyUint32(ctx, terms, i, JS_NewFloat64(ctx, double(matrix.m[i % 4][i / 4])));
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, name, terms);
    JS_FreeValue(ctx, global);
}

// Appends the name of each phase that an edit sink is told of to the sink's context, a string.
void recordPhase(void* context, PrLayoutNode, PrUiEditPhase phase) {
    const char* const names[] = {"begin ", "change ", "commit "}; // by PrUiEditPhase
    static_cast<String*>(context)->append(names[phase]);
}

// Returns the string that the script's expression `source` evaluates to.
String textOf(JsKitScripted& scripted, const char* source) {
    JSValue result = JS_Eval(scripted.ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
    size_t size = 0;
    const char* chars = JS_ToCStringLen(scripted.ctx, &size, result);
    REQUIRE(chars != nullptr);
    String copy(PrStringView{chars, size});
    JS_FreeCString(scripted.ctx, chars);
    JS_FreeValue(scripted.ctx, result);
    return copy;
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a box, a stack and a layer lay out the nodes under them as their attributes say") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {
            direction: 'row', justify: 'space-between', align: 'center', gap: '12', padding: '8 16 24 32', height: '80',
        });
        make('label', row, {}, 'first');
        make('divider', row, {});
        make('label', row, {}, 'second');
        var ends = make('box', 0, {direction: 'row'});
        make('label', ends, {}, 'left');
        make('spacer', ends, {});
        make('label', ends, {}, 'right');
        var lines = make('box', 0, {direction: 'row', wrap: '', gap: '4', width: '120', margin: '8'});
        ['one', 'two', 'three', 'four', 'five', 'six'].forEach(function (word) { make('badge', lines, {}, word); });
        var scroller = make('box', 0, {overflow: 'scroll-y', height: '40', 'min-width': '200', 'align-self': 'start'});
        ['a', 'b', 'c', 'd'].forEach(function (line) { make('label', scroller, {}, 'line ' + line); });
        var off = make('box', 0, {disabled: '', align: 'start'});
        make('button', off, {}, 'Disabled');
        var stack = make('stack', 0, {width: '200', height: '100', padding: '10', flex: 'fixed'});
        make('label', stack, {}, 'under');
        var layer = make('layer', stack, {justify: 'end', align: 'end', padding: '4'});
        make('badge', layer, {variant: 'accent'}, 'over');
    )JS");
    KitFixture direct;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .justify = PrKitJustify_SpaceBetween,
            .align = PrKitAlign_Center,
            .gap = 12,
            .padding = PrKitEdges{8, 16, 24, 32},
            .place = {.height = prKitPx(80)},
        });
        prKitLabel(kit, row, {.text = "first"}, {});
        prKitDivider(kit, row, {});
        prKitLabel(kit, row, {.text = "second"}, {});
        PrLayoutNode ends = prKitBox(kit, root, {}, {.direction = PrKitDirection_Row});
        prKitLabel(kit, ends, {.text = "left"}, {});
        prKitSpacer(kit, ends, {});
        prKitLabel(kit, ends, {.text = "right"}, {});
        PrLayoutNode lines = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .wrap = true,
            .gap = 4,
            .place = {.width = prKitPx(120), .margin = prKitEdgesAll(8)},
        });
        const char* words[] = {"one", "two", "three", "four", "five", "six"};
        for (const char* word : words)
            prKitBadge(kit, lines, {.text = word}, {});
        PrLayoutNode scroller = prKitBox(kit, root, {}, {
            .overflow = PrKitOverflow_ScrollY,
            .place = {.height = prKitPx(40), .minWidth = prKitPx(200), .align = PrKitAlign_Start},
        });
        const char* texts[] = {"line a", "line b", "line c", "line d"};
        for (const char* text : texts)
            prKitLabel(kit, scroller, {.text = text}, {});
        PrLayoutNode off = prKitBox(kit, root, {.disabled = true}, {.align = PrKitAlign_Start});
        prKitButton(kit, off, {.text = "Disabled"}, {});
        PrLayoutNode stack = prKitStack(kit, root, {}, {
            .padding = prKitEdgesAll(10),
            .place = {.width = prKitPx(200), .height = prKitPx(100), .flex = PrKitFlex_Fixed},
        });
        prKitLabel(kit, stack, {.text = "under"}, {});
        PrLayoutNode layer = prKitLayer(kit, stack, {}, {
            .justify = PrKitJustify_End,
            .align = PrKitAlign_End,
            .padding = prKitEdgesAll(4),
        });
        prKitBadge(kit, layer, {.text = "over"}, {.variant = PrKitBadgeVariant_Accent});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(scripted.fixture.paintedCommands() == direct.paintedCommands());
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a plane stands under the transform and the perspective that a script gives, and takes input where it is drawn") {
    JsKitScripted scripted;
    PrMatrix4 turn = prMatrix4Rotate(PrVec3{0, 1, 0}, 1.0f);
    defineMatrix(scripted, "turn", turn);
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var clicks = 0;
        var stage = make('plane', 0, {perspective: '300', padding: '40 0 0 80', width: '400', height: '300', flex: 'fixed'});
        var panel = make('plane', stage, {
            transform: turn.join(' '), align: 'start', gap: '8', padding: '12', width: '240', 'align-self': 'start',
        });
        make('label', panel, {}, 'A panel turned about its vertical axis');
        var button = make('button', panel, {}, 'Press');
        kitTree.listen(button, 'click', true);
        kitTree.onevent = function (node, type) { clicks += node === button && type === 'click' ? 1 : 0; };
    )JS");
    KitFixture direct;
    const PrMatrix4* transform = &turn;
    PrLayoutNode button = kPrLayoutNodeNone;
    uint32_t clicks = 0;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode stage = prKitPlane(kit, root, {}, {
            .box = {
                .padding = PrKitEdges{40, 0, 0, 80},
                .place = {.width = prKitPx(400), .height = prKitPx(300), .flex = PrKitFlex_Fixed},
            },
            .perspective = 300,
        });
        PrLayoutNode panel = prKitPlane(kit, stage, {.transform = transform}, {
            .box = {
                .align = PrKitAlign_Start,
                .gap = 8,
                .padding = prKitEdgesAll(12),
                .place = {.width = prKitPx(240), .align = PrKitAlign_Start},
            },
        });
        prKitLabel(kit, panel, {.text = "A panel turned about its vertical axis"}, {});
        button = prKitButton(kit, panel, {.text = "Press"}, {});
        clicks += prKitActivated(kit, button) ? 1 : 0;
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A transform that the plane refuses leaves it turned.
    scripted.run("var refused = kitTree.set(panel, 'transform', '1 0 0 1 0 0');");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A shift, with terms that a script writes with exponents. The button takes a click where the shift draws it.
    PrMatrix4 shift = prMatrix4Translate(PrVec3{25, 10, 0});
    transform = &shift;
    scripted.run("kitTree.set(panel, 'transform', '1 0 0 0 0 1 0 0 0 0 1 0 2.5e1 1E+1 0 1');");
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    PrVec2 drawn = direct.center(button) + PrVec2{25, 10};
    scripted.fixture.click(drawn);
    direct.click(drawn);
    frames(scripted, direct);
    CHECK(clicks == 1);
    CHECK(scripted.number("clicks") == 1);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // Reset, the plane has no transform.
    transform = nullptr;
    scripted.run("kitTree.set(panel, 'transform', null);");
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a plane's level orders it, a plane that hides its back shows nothing of it, and a disabled plane disables") {
    JsKitScripted scripted;
    PrMatrix4 flip = prMatrix4Rotate(PrVec3{0, 1, 0}, 3.1415927f);
    defineMatrix(scripted, "flip", flip);
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var stack = make('stack', 0, {width: '200', height: '80', flex: 'fixed'});
        var lifted = make('plane', stack, {'z-index': '1', width: '100%', height: '100%'});
        var above = make('panel', lifted, {background: 'raised', width: '100%', height: '100%'});
        make('label', above, {}, 'lifted above the layer');
        var layer = make('layer', stack, {});
        var cover = make('panel', layer, {background: 'select', width: '100%', height: '100%'});
        make('label', cover, {}, 'the layer');
        var flipped = make('plane', 0, {transform: flip.join(' '), 'backface-hidden': '', width: '200'});
        make('label', flipped, {}, 'the back of a plane');
        var off = make('plane', 0, {disabled: '', direction: 'row', justify: 'end', gap: '4', overflow: 'clip', wrap: ''});
        make('button', off, {}, 'One');
        make('button', off, {}, 'Two');
    )JS");
    KitFixture direct;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrKitBoxStyle cover{.place = {.width = prKitPercent(100), .height = prKitPercent(100)}};
        PrLayoutNode stack = prKitStack(kit, root, {}, {
            .place = {.width = prKitPx(200), .height = prKitPx(80), .flex = PrKitFlex_Fixed},
        });
        PrLayoutNode lifted = prKitPlane(kit, stack, {}, {.box = cover, .zIndex = 1});
        PrLayoutNode above = prKitPanel(kit, lifted, {}, {.box = cover, .background = PrKitBackground_Raised});
        prKitLabel(kit, above, {.text = "lifted above the layer"}, {});
        PrLayoutNode layer = prKitLayer(kit, stack, {}, {});
        PrLayoutNode covering = prKitPanel(kit, layer, {}, {.box = cover, .background = PrKitBackground_Select});
        prKitLabel(kit, covering, {.text = "the layer"}, {});
        PrLayoutNode flipped = prKitPlane(kit, root, {.transform = &flip}, {
            .box = {.place = {.width = prKitPx(200)}},
            .backfaceHidden = true,
        });
        prKitLabel(kit, flipped, {.text = "the back of a plane"}, {});
        PrLayoutNode off = prKitPlane(kit, root, {.disabled = true}, {
            .box = {
                .direction = PrKitDirection_Row,
                .justify = PrKitJustify_End,
                .overflow = PrKitOverflow_Clip,
                .wrap = true,
                .gap = 4,
            },
        });
        prKitButton(kit, off, {.text = "One"}, {});
        prKitButton(kit, off, {.text = "Two"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a box hears the gestures of the controls under it, each phase on its side of the control's change") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var names = {};
        var heard = [];
        kitTree.onevent = function (node, type) {
            heard.push(names[node] + '.' + type);
        };
        function hear(name, node, types) {
            names[node] = name;
            for (const type of types) {
                if (!kitTree.listen(node, type, true))
                    throw new Error('no ' + type + ' event');
            }
        }
        var outer = make('box', 0, {align: 'start'});
        var row = make('box', outer, {direction: 'row', align: 'center', gap: '8'});
        var slider = make('slider', row, {value: '0.25', width: '200', flex: 'fixed'});
        var toggle = make('toggle', row, {});
        var apart = make('toggle', outer, {});
        hear('outer', outer, ['editcommit']);
        hear('row', row, ['editbegin', 'editchange', 'editcommit']);
        hear('slider', slider, ['change']);
        hear('toggle', toggle, ['change']);
        hear('apart', apart, ['change']);
    )JS");
    KitFixture direct;
    String phases;
    float value = 0.25f;
    bool on = false;
    bool other = false;
    PrLayoutNode slider = kPrLayoutNodeNone;
    PrLayoutNode toggle = kPrLayoutNodeNone;
    PrLayoutNode apart = kPrLayoutNodeNone;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode outer = prKitBox(kit, root, {}, {.align = PrKitAlign_Start});
        PrLayoutNode row = prKitBox(kit, outer, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
        });
        prUiSetEditSink(kit->ui, row, recordPhase, &phases);
        slider = prKitSlider(kit, row, {.value = prKitScalarF32(&value)}, {});
        prKitPlace(kit, slider, {.width = prKitPx(200), .flex = PrKitFlex_Fixed});
        toggle = prKitToggle(kit, row, {.value = &on}, {});
        apart = prKitToggle(kit, outer, {.value = &other}, {});
    };
    auto push = [&](PrAppEvent event) {
        scripted.fixture.push(event);
        direct.push(event);
    };
    auto click = [&](PrLayoutNode node) {
        scripted.fixture.click(direct.center(node));
        direct.click(direct.center(node));
        frames(scripted, direct);
    };
    frames(scripted, direct, 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A drag is one gesture. Its begin comes before the change of its first write, and each later phase comes after the
    // change of the round that reported the phase.
    PrVec2 at = direct.center(slider);
    push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = at}});
    frames(scripted, direct);
    at.x += 40;
    push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = at}});
    frames(scripted, direct);
    push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = at}});
    frames(scripted, direct);
    CHECK(phases == "begin change change commit ");
    CHECK(textOf(scripted, "heard.join(' ')") ==
        "row.editbegin slider.change row.editchange slider.change row.editchange row.editcommit");

    // A toggle's gesture is whole in one round, around the toggle's change.
    scripted.run("heard = [];");
    phases.clear();
    click(toggle);
    CHECK(on);
    CHECK(phases == "begin change commit ");
    CHECK(textOf(scripted, "heard.join(' ')") == "row.editbegin toggle.change row.editchange row.editcommit");

    // A gesture reaches the nearest box that listens, and that box hears only the phases that it listens for.
    scripted.run("heard = [];");
    click(apart);
    CHECK(other);
    CHECK(textOf(scripted, "heard.join(' ')") == "apart.change outer.editcommit");
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A box that the script no longer listens on leaves the gestures to the box above it.
    scripted.run(R"JS(
        heard = [];
        for (const type of ['editbegin', 'editchange', 'editcommit'])
            kitTree.listen(row, type, false);
    )JS");
    click(toggle);
    CHECK(textOf(scripted, "heard.join(' ')") == "toggle.change outer.editcommit");
}

TEST_CASE("the kinds of boxes take only their own attributes, and a box alone takes events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var box = kitTree.create('box');
        var plane = kitTree.create('plane');
        var stack = kitTree.create('stack');
        var layer = kitTree.create('layer');
        var spacer = kitTree.create('spacer');
        var divider = kitTree.create('divider');
        var identity = '1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1';
        var taken = [
            kitTree.set(plane, 'transform', identity), kitTree.set(plane, 'transform', '  ' + identity + ' '),
            kitTree.set(plane, 'transform', '1 0 0 0 0 1 0 0 0 0 1 0 -1e-7 +2.5E+1 0.5 1'),
            kitTree.set(plane, 'transform', null), kitTree.set(plane, 'z-index', '-3'),
            kitTree.set(plane, 'z-index', null), kitTree.set(plane, 'perspective', '0.5'),
            kitTree.set(plane, 'backface-hidden', 'false'), kitTree.set(plane, 'gap', '4'),
            kitTree.set(stack, 'justify', 'space-evenly'), kitTree.set(layer, 'overflow', 'scroll'),
            kitTree.set(layer, 'disabled', ''), kitTree.set(spacer, 'weight', '2'),
            kitTree.listen(box, 'editbegin', true), kitTree.listen(box, 'editchange', true),
            kitTree.listen(box, 'editcommit', true),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(plane, 'transform', '1 0 0 0 0 1 0 0 0 0 1 0 0 0 0'),
            kitTree.set(plane, 'transform', identity + ' 1'), kitTree.set(plane, 'transform', ''),
            kitTree.set(plane, 'transform', identity.replace('1', 'one')),
            kitTree.set(plane, 'transform', identity.replace('1', '1e999')),
            kitTree.set(plane, 'transform', identity.replace(/ /g, ',')),
            kitTree.set(plane, 'z-index', '1.5'), kitTree.set(plane, 'z-index', '2147483648'),
            kitTree.set(plane, 'perspective', '-1'), kitTree.set(plane, 'backface-hidden', 'yes'),
            kitTree.set(plane, 'direction', 'diagonal'), kitTree.set(stack, 'transform', identity),
            kitTree.set(layer, 'z-index', '1'), kitTree.set(spacer, 'gap', '4'),
            kitTree.set(divider, 'direction', 'row'), kitTree.listen(plane, 'click', true),
            kitTree.listen(stack, 'click', true), kitTree.listen(box, 'click', true),
            kitTree.listen(stack, 'editbegin', true), kitTree.listen(layer, 'editcommit', true),
            kitTree.listen(plane, 'editchange', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 16);
    CHECK(scripted.number("refused") == 0);
}
