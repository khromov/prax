// The kinds of Studio Kit's text in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/kit_box.h"
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

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a label sets its text in the role, tone, wrap and alignment that a script names") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var column = make('box', 0, {align: 'start', gap: '4', padding: '8'});
        make('label', column, {role: 'mono', tone: 'danger'}, 'error: no such node');
        make('label', column, {wrap: 'clip', width: '80'}, 'A label that is cut at the edge of its box');
        make('label', column, {wrap: 'nowrap', width: '80'}, 'A label that runs past its box');
        make('label', column, {'text-align': 'center', width: '200'}, 'centered');
        make('label', column, {'text-align': 'end', width: '200', role: 'caption'}, 'AT THE END');
        make('label', column, {preserve: '', role: 'mono-strong'}, 'two  spaces\nand a line feed');
        var holder = make('label', column, {tone: 'accent'}, 'A label holds what is under it: ');
        make('label', holder, {role: 'strong'}, 'another label');
    )JS");
    KitFixture direct;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode column =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 4, .padding = prKitEdgesAll(8)});
        prKitLabel(kit, column, {.text = "error: no such node"},
            {.role = PrKitTextRole_Mono, .tone = PrKitTone_Danger});
        PrLayoutNode clipped =
            prKitLabel(kit, column, {.text = "A label that is cut at the edge of its box"}, {.wrap = PrKitWrap_Clip});
        prKitPlace(kit, clipped, {.width = prKitPx(80)});
        PrLayoutNode past =
            prKitLabel(kit, column, {.text = "A label that runs past its box"}, {.wrap = PrKitWrap_Nowrap});
        prKitPlace(kit, past, {.width = prKitPx(80)});
        PrLayoutNode centered = prKitLabel(kit, column, {.text = "centered"}, {.align = PrKitTextAlign_Center});
        prKitPlace(kit, centered, {.width = prKitPx(200)});
        PrLayoutNode ended = prKitLabel(kit, column, {.text = "AT THE END"}, {
            .role = PrKitTextRole_Caption,
            .align = PrKitTextAlign_End,
        });
        prKitPlace(kit, ended, {.width = prKitPx(200)});
        prKitLabel(kit, column, {.text = "two  spaces\nand a line feed"}, {
            .role = PrKitTextRole_MonoStrong,
            .preserve = true,
        });
        PrLayoutNode holder =
            prKitLabel(kit, column, {.text = "A label holds what is under it: "}, {.tone = PrKitTone_Accent});
        prKitLabel(kit, holder, {.text = "another label"}, {.role = PrKitTextRole_Strong});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a paragraph sets the runs under it, each in its own role and tone, and follows the script's edits of them") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var paragraph = make('text', 0, {
            role: 'body-small', tone: 'muted', wrap: 'wrap', 'text-align': 'center', width: '180', margin: '8',
        }, 'Drag ');
        var pan = make('run', paragraph, {}, 'to pan, hold ');
        var shift = make('run', paragraph, {role: 'mono-strong', tone: 'accent'}, 'Shift');
        var snap = make('run', paragraph, {tone: 'secondary'}, ' to snap to the grid.');
        make('label', paragraph, {}, 'A label under a paragraph is not declared');
        make('label', shift, {}, 'and neither is a node under a run');
        make('run', 0, {role: 'title'}, 'A run under no paragraph declares nothing');
        var kept = make('text', 0, {preserve: '', role: 'mono'});
        make('run', kept, {role: 'mono'}, 'x  =  1\n');
        make('run', kept, {role: 'mono', tone: 'faint'}, '// kept as written');
        make('text', 0, {tone: 'warning'}, 'A paragraph of its own text alone');
        make('text', 0, {});
    )JS");
    KitFixture direct;
    Array<PrKitTextRun> runs;
    runs.push({.text = "Drag ", .role = PrKitTextRole_BodySmall, .tone = PrKitTone_Muted});
    runs.push({.text = "to pan, hold "});
    runs.push({.text = "Shift", .role = PrKitTextRole_MonoStrong, .tone = PrKitTone_Accent});
    runs.push({.text = " to snap to the grid.", .tone = PrKitTone_Secondary});
    PrKitTextStyle style{
        .role = PrKitTextRole_BodySmall,
        .tone = PrKitTone_Muted,
        .wrap = PrKitWrap_Wrap,
        .align = PrKitTextAlign_Center,
    };
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode paragraph = prKitText(kit, root, {.runs = runs.data(), .count = runs.count()}, style);
        prKitPlace(kit, paragraph, {.width = prKitPx(180), .margin = prKitEdgesAll(8)});
        const PrKitTextRun kept[] = {
            {.text = "x  =  1\n", .role = PrKitTextRole_Mono},
            {.text = "// kept as written", .role = PrKitTextRole_Mono, .tone = PrKitTone_Faint},
        };
        prKitText(kit, root, {.runs = kept, .count = 2}, {.role = PrKitTextRole_Mono, .preserve = true});
        const PrKitTextRun own = {.text = "A paragraph of its own text alone", .tone = PrKitTone_Warning};
        prKitText(kit, root, {.runs = &own, .count = 1}, {.tone = PrKitTone_Warning});
        prKitText(kit, root, {}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A run's text and attributes change, and the paragraph's own text goes.
    scripted.run(R"JS(
        kitTree.setText(shift, 'Control');
        kitTree.set(shift, 'tone', 'danger');
        kitTree.set(snap, 'role', 'body-italic');
        kitTree.setText(paragraph, '');
    )JS");
    runs.erase(0);
    runs[1].text = "Control";
    runs[1].tone = PrKitTone_Danger;
    runs[2].role = PrKitTextRole_BodyItalic;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The runs are those under the paragraph, in the tree's order: one moves first, one leaves, and one is new.
    scripted.run(R"JS(
        kitTree.insert(paragraph, snap, pan);
        kitTree.destroy(shift);
        make('run', paragraph, {role: 'strong'}, ' Done.');
        kitTree.set(paragraph, 'text-align', null);
        kitTree.set(paragraph, 'tone', 'primary');
    )JS");
    PrKitTextRun pan = runs[0];
    PrKitTextRun snap = runs[2];
    runs.clear();
    runs.push(snap);
    runs.push(pan);
    runs.push({.text = " Done.", .role = PrKitTextRole_Strong});
    style.align = PrKitTextAlign_Start;
    style.tone = PrKitTone_Primary;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("the kinds of text take only their own attributes, and no events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var label = kitTree.create('label');
        var text = kitTree.create('text');
        var run = kitTree.create('run');
        var taken = [
            kitTree.set(text, 'role', 'display'), kitTree.set(text, 'wrap', 'clip'), kitTree.set(text, 'preserve', ''),
            kitTree.set(text, 'text-align', 'end'), kitTree.set(text, 'max-width', '50%'),
            kitTree.set(run, 'role', 'mono-num'), kitTree.set(run, 'tone', 'axis-x'), kitTree.set(run, 'tone', null),
            kitTree.set(label, 'text-align', 'center'),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(text, 'role', 'huge'), kitTree.set(text, 'wrap', ''), kitTree.set(text, 'text-align', 'justify'),
            kitTree.set(text, 'runs', '2'), kitTree.set(run, 'wrap', 'wrap'), kitTree.set(run, 'text-align', 'end'),
            kitTree.set(run, 'preserve', ''), kitTree.set(run, 'tone', 'loud'), kitTree.set(label, 'align', 'center'),
            kitTree.listen(label, 'click', true), kitTree.listen(text, 'click', true), kitTree.listen(run, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 9);
    CHECK(scripted.number("refused") == 0);
}
