// Studio Kit's QuickJS binding: the tree that a script edits declares what the same calls to the kit declare, and a
// widget's input reaches the script between rounds.

#include "pr/format.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_input.h"
#include "pr/kit_text.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// The widgets that the tests' scripts make, declared by the kit's own calls, for `clicks` activations of the button.
// Returns the button.
PrLayoutNode declareCounter(PrKit* kit, PrLayoutNode root, uint32_t clicks) {
    PrLayoutNode page = prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 16, .padding = prKitEdgesXY(8, 32)});
    prKitLabel(kit, page, {.text = "Counter"}, {.role = PrKitTextRole_Title, .tone = PrKitTone_Secondary});
    PrLayoutNode row = prKitBox(kit, page, {}, {.direction = PrKitDirection_Row, .align = PrKitAlign_Center, .gap = 8});
    PrLayoutNode button = prKitButton(kit, row, {.text = "Add"}, {.variant = PrKitButtonVariant_Primary});
    PrKitBadgeStyle badge{};
    if (clicks > 0)
        badge.variant = PrKitBadgeVariant_Accent;
    prKitBadge(kit, row, {.text = clicks > 0 ? "some" : "none"}, badge);
    PrLayoutNode note = prKitLabel(kit, page, {.text = "A label that its caller places, which wraps at its width."}, {});
    prKitPlace(kit, note, {.width = prKitPx(120)});
    return button;
}

// The script that makes the widgets of declareCounter, and counts the button's clicks in `clicks`.
const char* const kCounterScript = R"JS(
    var clicks = 0;
    var page = make('box', 0, {align: 'start', gap: '16', padding: '8 32'});
    make('label', page, {role: 'title', tone: 'secondary'}, 'Counter');
    var row = make('box', page, {direction: 'row', align: 'center', gap: '8'});
    var button = make('button', row, {variant: 'primary'}, 'Add');
    var badge = make('badge', row, {}, 'none');
    make('label', page, {width: '120'}, 'A label that its caller places, which wraps at its width.');
    kitTree.listen(button, 'click', true);
    kitTree.onevent = function (node, type) {
        if (node !== button || type !== 'click')
            throw new Error('an event of another node');
        clicks += 1;
        kitTree.setText(badge, 'some');
        kitTree.set(badge, 'variant', 'accent');
    };
)JS";

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a tree that a script makes renders as the same calls to the kit do, before and after a click") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kCounterScript);
    KitFixture direct;
    PrLayoutNode button = kPrLayoutNodeNone;
    uint32_t clicks = 0;
    direct.build = [&](PrLayoutNode root) {
        button = declareCounter(direct.kit, root, clicks);
        clicks += prKitActivated(direct.kit, button) ? 1 : 0;
    };
    scripted.fixture.frame();
    direct.frame();
    CHECK(kitVerified(scripted.fixture.tree));
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(scripted.fixture.paintedCommands() == direct.paintedCommands());

    // The same click on each. The script hears of it after the round that routed it, and the frame's later rounds
    // declare what the script changed, so both frames end with the badge changed.
    PrVec2 at = direct.center(button);
    scripted.fixture.click(at);
    direct.click(at);
    scripted.fixture.frame();
    direct.frame();
    CHECK(clicks == 1);
    CHECK(scripted.number("clicks") == 1);
    // The pointer has left the button in neither, so both show it hovered, and a later frame settles both alike.
    for (uint32_t i = 0; i < 30; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a node that a script moves, removes or destroys is declared where the tree has it") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var column = kitTree.create('box');
        kitTree.insert(0, column, 0);
        var labels = ['first', 'second', 'third'].map(function (text) {
            const label = kitTree.create('label');
            kitTree.setText(label, text);
            kitTree.insert(column, label, 0);
            return label;
        });
    )JS");
    KitFixture direct;
    const char* order[3] = {"first", "second", "third"};
    uint32_t count = 3;
    direct.build = [&](PrLayoutNode root) {
        PrLayoutNode column = prKitBox(direct.kit, root, {}, {});
        for (uint32_t i = 0; i < count; ++i)
            prKitLabel(direct.kit, column, {.text = order[i]}, {});
    };
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The last label moves before the first, as an insert of an attached node moves it.
    scripted.run("kitTree.insert(column, labels[2], labels[0]);");
    order[0] = "third";
    order[1] = "first";
    order[2] = "second";
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A removed node is declared no more, and neither is a destroyed one, whose number names nothing afterward.
    scripted.run("kitTree.remove(labels[1]); kitTree.destroy(labels[0]);");
    count = 1;
    scripted.fixture.frame();
    direct.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run(R"JS(
        var threw = 0;
        try { kitTree.setText(labels[0], 'gone'); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.insert(labels[2], column, 0); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.destroy(0); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.create('no-such-kind'); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
    )JS");
    CHECK(scripted.number("threw") == 4);
}

TEST_CASE("an attribute takes only the values of its kind, and a value that does not suit it changes nothing") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var box = kitTree.create('box');
        var label = kitTree.create('label');
        var button = kitTree.create('button');
        var taken = [
            kitTree.set(box, 'padding', '4 8 12'), kitTree.set(box, 'wrap', ''), kitTree.set(box, 'width', '50%'),
            kitTree.set(label, 'wrap', 'nowrap'), kitTree.set(label, 'max-width', 'fill'),
            kitTree.set(button, 'size', 'lg'), kitTree.set(button, 'size', null), kitTree.set(box, 'gap', '2.5e-1'),
            kitTree.listen(button, 'click', true),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(box, 'padding', '4 8 12 16 20'), kitTree.set(box, 'gap', '-1'), kitTree.set(box, 'role', 'title'),
            kitTree.set(label, 'wrap', ''), kitTree.set(label, 'variant', 'primary'), kitTree.set(label, 'width', '1e'),
            kitTree.set(button, 'size', 'huge'), kitTree.set(0, 'gap', '4'),
            kitTree.listen(button, 'keydown', true), kitTree.listen(label, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 9);
    CHECK(scripted.number("refused") == 0);
}

TEST_CASE("a node tells where its widget was laid out, and is brought into view when the script asks") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var column = make('box', 0, {overflow: 'scroll-y', height: '200', gap: '8'});
        var rows = [];
        for (var i = 0; i < 40; ++i)
            rows.push(make('label', column, {}, 'row ' + i));
        var told = {};
        var count = 0;
        kitTree.listen(rows[0], 'bounds', true);
        kitTree.listen(rows[39], 'bounds', true);
        kitTree.onevent = function (node, type, detail) {
            if (type !== 'bounds' || detail.values.length !== 4)
                throw new Error('an event that is not a box');
            told[node] = detail.values;
            count += 1;
        };
    )JS");
    KitFixture direct;
    PrLayoutNode first = kPrLayoutNodeNone;
    PrLayoutNode last = kPrLayoutNodeNone;
    bool reveal = false;
    direct.build = [&](PrLayoutNode root) {
        PrKitBoxStyle style{.overflow = PrKitOverflow_ScrollY, .gap = 8};
        style.place.height = prKitPx(200);
        PrLayoutNode column = prKitBox(direct.kit, root, {}, style);
        char text[16];
        for (uint32_t i = 0; i < 40; ++i) {
            PrLayoutNode row = prKitLabel(direct.kit, column, {.text = formatToBuffer(Span(text), "row {}", i)}, {});
            if (i == 0)
                first = row;
            if (i == 39)
                last = row;
        }
        if (reveal)
            prUiReveal(direct.kit->ui, last);
        reveal = false;
    };
    for (uint32_t i = 0; i < 3; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
    // Each row told its box once it was laid out, and nothing more while it stayed where it was.
    PrRect firstBox = direct.bounds(first);
    PrRect lastBox = direct.bounds(last);
    CHECK(scripted.number("count") == 2);
    CHECK(scripted.number("told[rows[0]][1]") == doctest::Approx(double(firstBox.top)));
    CHECK(scripted.number("told[rows[0]][3]") == doctest::Approx(double(firstBox.bottom)));
    CHECK(scripted.number("told[rows[39]][1]") == doctest::Approx(double(lastBox.top)));
    CHECK(lastBox.top > 200);

    // The last row is out of view until it is revealed, which moves every row, and each tells its new box.
    scripted.run("kitTree.set(rows[39], 'reveal', '1');");
    reveal = true;
    for (uint32_t i = 0; i < 30; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
    lastBox = direct.bounds(last);
    CHECK(lastBox.bottom <= 200.5f);
    CHECK(scripted.number("told[rows[39]][3]") == doctest::Approx(double(lastBox.bottom)));
    CHECK(scripted.number("told[rows[0]][1]") == doctest::Approx(double(direct.bounds(first).top)));
    CHECK(scripted.number("told[rows[0]][1]") < 0);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The same value reveals nothing more, and a script that stops listening hears nothing more.
    double heard = scripted.number("count");
    scripted.run("kitTree.listen(rows[0], 'bounds', false); kitTree.set(rows[0], 'reveal', '1');");
    reveal = false;
    direct.build = [&](PrLayoutNode root) {
        PrKitBoxStyle style{.overflow = PrKitOverflow_ScrollY, .gap = 8};
        style.place.height = prKitPx(200);
        PrLayoutNode column = prKitBox(direct.kit, root, {}, style);
        char text[16];
        for (uint32_t i = 0; i < 40; ++i) {
            PrLayoutNode row = prKitLabel(direct.kit, column, {.text = formatToBuffer(Span(text), "row {}", i)}, {});
            if (i == 0 && reveal)
                prUiReveal(direct.kit->ui, row);
            if (i == 0)
                first = row;
        }
        reveal = false;
    };
    reveal = true;
    for (uint32_t i = 0; i < 30; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
    CHECK(direct.bounds(first).top >= 0);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(scripted.number("count") == heard + 1);
}

TEST_CASE("a widget that a script makes a drag source carries its payload as one that a program makes a source does") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '8', padding: '16'});
        var cube = make('button', row, {'drag-source': '7', 'drag-value': '1'}, 'Cube');
        var sphere = make('button', row, {'drag-source': '7', 'drag-value': '9007199254740993'}, 'Sphere');
        var grip = make('button', row, {'drag-source': '7', 'drag-value': '3', 'drag-threshold': '0'}, 'Grip');
        make('label', row, {'drag-source': '7', 'drag-value': '4', 'drag-threshold': '0'}, 'Plain');
        var field = make('asset-field', 0, {kind: '7'}, 'None');
        var clicks = 0;
        var drops = [];
        [cube, sphere, grip].forEach(function (button) { kitTree.listen(button, 'click', true); });
        kitTree.listen(field, 'drop', true);
        kitTree.onevent = function (node, type, detail) {
            if (type === 'click')
                clicks += 1;
            else
                drops.push(detail.text);
        };
        // A value that an attribute refuses leaves the source as it was.
        var refused = [
            kitTree.set(cube, 'drag-source', 'cube'), kitTree.set(cube, 'drag-source', '-7'),
            kitTree.set(cube, 'drag-source', '7.5'), kitTree.set(cube, 'drag-source', ''),
            kitTree.set(cube, 'drag-source', '18446744073709551615'), kitTree.set(cube, 'drag-value', 'first'),
            kitTree.set(cube, 'drag-value', '-1'), kitTree.set(cube, 'drag-value', ''),
            kitTree.set(cube, 'drag-threshold', '-4'), kitTree.set(cube, 'drag-threshold', 'far'),
            kitTree.set(0, 'drag-source', '7'),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("refused") == 0);

    // A payload's value past the whole numbers that a script's number holds.
    const uint64_t kWideValue = 9007199254740993ull;
    struct Source {
        const char* name;
        uint64_t value;
        float threshold;
    };
    Source sources[] = {{"Cube", 1, 4}, {"Sphere", kWideValue, 4}, {"Grip", 3, 0}};
    KitFixture direct;
    PrLayoutNode buttons[3] = {};
    PrLayoutNode label = kPrLayoutNodeNone;
    PrLayoutNode field = kPrLayoutNodeNone;
    bool cubeDrags = true;
    uint32_t clicks = 0;
    uint32_t drops = 0;
    uint64_t dropped = 0;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrKitBoxStyle line{
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
            .padding = prKitEdgesAll(16),
        };
        PrLayoutNode row = prKitBox(kit, root, {}, line);
        for (uint32_t i = 0; i < 3; ++i) {
            buttons[i] = prKitButton(kit, row, {.text = sources[i].name}, {});
            clicks += prKitActivated(kit, buttons[i]) ? 1 : 0;
            if (i != 0 || cubeDrags)
                prUiDragStart(kit->ui, buttons[i], sources[i].threshold, 7, sources[i].value, 0);
        }
        // A label takes no presses, so it picks nothing up.
        label = prKitLabel(kit, row, {.text = "Plain"}, {});
        prUiDragStart(kit->ui, label, 0, 7, 4, 0);
        field = prKitAssetField(kit, root, {.name = "None", .kind = 7});
        uint64_t value = 0;
        if (prKitDropped(kit, field, &value)) {
            dropped = value;
            ++drops;
        }
    };
    auto frame = [&](uint32_t count = 1) {
        for (uint32_t i = 0; i < count; ++i) {
            scripted.fixture.frame();
            direct.frame();
        }
    };
    auto push = [&](PrAppEventKind kind, PrVec2 at) {
        PrAppEvent event{.kind = kind, .pointer = {.position = at}};
        scripted.fixture.push(event);
        direct.push(event);
    };
    // Returns true if both carry a payload of the same kind and value, from widgets with the same bounds, or neither
    // carries one.
    auto carryAlike = [&]() {
        const PrUiPayload* a = prUiDragging(scripted.fixture.ui);
        const PrUiPayload* b = prUiDragging(direct.ui);
        if (a == nullptr || b == nullptr)
            return a == b;
        PrRect from = scripted.fixture.bounds(a->source);
        PrRect to = direct.bounds(b->source);
        return a->kind == b->kind && a->value == b->value && prMemEquals(&from, &to, sizeof(from));
    };
    frame(2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A press on Sphere that moves less than the 4 px of a source with no threshold picks nothing up.
    PrVec2 start = direct.center(buttons[1]);
    push(PrAppEventKind_PointerDown, start);
    frame();
    push(PrAppEventKind_PointerMove, PrVec2{start.x + 3, start.y});
    frame(2);
    CHECK(prUiDragging(direct.ui) == nullptr);
    CHECK(carryAlike());

    // Past the threshold it carries the payload, which marks the field that takes its kind and drops on it.
    push(PrAppEventKind_PointerMove, PrVec2{start.x + 5, start.y});
    frame();
    REQUIRE(prUiDragging(direct.ui) != nullptr);
    CHECK(prUiDragging(direct.ui)->source == buttons[1]);
    CHECK(prUiDragging(direct.ui)->value == kWideValue);
    CHECK(carryAlike());
    PrVec2 over = direct.center(field);
    push(PrAppEventKind_PointerMove, over);
    frame(2);
    CHECK(carryAlike());
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    push(PrAppEventKind_PointerUp, over);
    frame(30);
    CHECK(drops == 1);
    CHECK(dropped == kWideValue);
    CHECK(scripted.number("drops.length") == 1);
    CHECK(scripted.number("drops[0] === '9007199254740993' ? 1 : 0") == 1);
    CHECK(carryAlike());
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The release that dropped the payload was no click, and a press that stays put is one.
    CHECK(clicks == 0);
    CHECK(scripted.number("clicks") == 0);
    start = direct.center(buttons[0]);
    scripted.fixture.click(start);
    direct.click(start);
    frame(2);
    CHECK(clicks == 1);
    CHECK(scripted.number("clicks") == 1);

    // Cube carries the value that the script gave it, which the values that it refused did not change.
    push(PrAppEventKind_PointerDown, start);
    frame();
    push(PrAppEventKind_PointerMove, PrVec2{start.x + 5, start.y});
    frame();
    REQUIRE(prUiDragging(direct.ui) != nullptr);
    CHECK(prUiDragging(direct.ui)->value == 1);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, PrVec2{start.x + 5, start.y});
    frame(2);

    // A threshold of zero picks the payload up as the press begins. A widget that takes no presses picks none up.
    start = direct.center(buttons[2]);
    push(PrAppEventKind_PointerDown, start);
    frame();
    REQUIRE(prUiDragging(direct.ui) != nullptr);
    CHECK(prUiDragging(direct.ui)->value == 3);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, start);
    frame(2);
    start = direct.center(label);
    push(PrAppEventKind_PointerDown, start);
    frame();
    CHECK(prUiDragging(direct.ui) == nullptr);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, start);
    frame(2);
    CHECK(clicks == 1);
    CHECK(scripted.number("clicks") == 1);

    // A source whose kind the script resets picks nothing up, and one whose value it sets carries the new value.
    scripted.run("kitTree.set(cube, 'drag-source', null); kitTree.set(sphere, 'drag-value', '2');");
    cubeDrags = false;
    start = direct.center(buttons[0]);
    push(PrAppEventKind_PointerDown, start);
    frame();
    push(PrAppEventKind_PointerMove, PrVec2{start.x + 40, start.y + 40});
    frame(2);
    CHECK(prUiDragging(direct.ui) == nullptr);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, PrVec2{start.x + 40, start.y + 40});
    frame(2);

    // A threshold that the script sets is the distance that picks the payload up. A press that ends short of it is a
    // click.
    scripted.run("kitTree.set(sphere, 'drag-threshold', '10');");
    sources[1] = Source{"Sphere", 2, 10};
    start = direct.center(buttons[1]);
    PrVec2 nearby{start.x + 8, start.y};
    push(PrAppEventKind_PointerDown, start);
    frame();
    push(PrAppEventKind_PointerMove, nearby);
    frame(2);
    CHECK(prUiDragging(direct.ui) == nullptr);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, nearby);
    frame(2);
    CHECK(clicks == 2);
    CHECK(scripted.number("clicks") == 2);

    // A threshold that the script resets is 4 px again.
    scripted.run("kitTree.set(sphere, 'drag-threshold', null);");
    sources[1].threshold = 4;
    push(PrAppEventKind_PointerDown, start);
    frame();
    push(PrAppEventKind_PointerMove, nearby);
    frame();
    REQUIRE(prUiDragging(direct.ui) != nullptr);
    CHECK(prUiDragging(direct.ui)->value == 2);
    CHECK(carryAlike());
    push(PrAppEventKind_PointerUp, nearby);
    frame(30);
    CHECK(drops == 1);
    CHECK(scripted.number("drops.length") == 1);
    CHECK(clicks == 2);
    CHECK(scripted.number("clicks") == 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a node's widget takes focus when the script asks, as a widget does that the kit's caller gives focus") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '8', padding: '8'});
        var first = make('button', row, {}, 'First');
        var second = make('button', row, {}, 'Second');
        var locked = make('button', row, {disabled: ''}, 'Locked');
        var note = make('label', row, {}, 'a label takes no focus');
        // The buttons that were activated, in order, as the digits of a number: 1 for the first, and 2 for the second.
        var clicks = 0;
        kitTree.listen(first, 'click', true);
        kitTree.listen(second, 'click', true);
        kitTree.onevent = function (node) {
            clicks = clicks * 10 + (node === first ? 1 : 2);
        };
    )JS");
    KitFixture direct;
    constexpr uint32_t kNone = 4;
    PrLayoutNode widgets[kNone] = {};
    uint32_t asked = kNone;         // the widget that the next round gives focus, or kNone
    uint32_t activations[2] = {};
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrKitBoxStyle style{
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        };
        PrLayoutNode row = prKitBox(kit, root, {}, style);
        widgets[0] = prKitButton(kit, row, {.text = "First"}, {});
        widgets[1] = prKitButton(kit, row, {.text = "Second"}, {});
        widgets[2] = prKitButton(kit, row, {.text = "Locked", .disabled = true}, {});
        widgets[3] = prKitLabel(kit, row, {.text = "a label takes no focus"}, {});
        for (uint32_t i = 0; i < 2; ++i)
            activations[i] += prKitActivated(kit, widgets[i]) ? 1 : 0;
        if (asked != kNone)
            prUiSetFocus(kit->ui, widgets[asked]);
        asked = kNone;
    };
    auto frames = [&](uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            scripted.fixture.frame();
            direct.frame();
        }
    };
    // Gives a widget focus in each, by the script's attribute and by the kit's call, and presses Enter, which activates
    // the widget that has focus.
    auto focusAndEnter = [&](const char* script, uint32_t widget) {
        scripted.run(script);
        asked = widget;
        frames(2);
        scripted.fixture.key(PrAppKey_Enter);
        direct.key(PrAppKey_Enter);
        frames(2);
    };
    frames(2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(prUiFocus(scripted.fixture.ui) == kPrLayoutNodeNone);

    // The key shows the ring of the focus that it found, which both draw around the second button.
    focusAndEnter("kitTree.set(second, 'focus', '1');", 1);
    CHECK(prUiFocus(direct.ui) == widgets[1]);
    CHECK(prUiFocus(scripted.fixture.ui) != kPrLayoutNodeNone);
    CHECK(activations[1] == 1);
    CHECK(scripted.number("clicks") == 2);
    frames(30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The round that gives a widget focus asks for a frame, since the widget shows its focus in a later round.
    CHECK(!prUiOutput(scripted.fixture.ui).wantsFrame);
    scripted.run("kitTree.set(first, 'focus', '0');");
    asked = 0;
    frames(1);
    CHECK(prUiOutput(scripted.fixture.ui).wantsFrame);
    frames(30);
    CHECK(!prUiOutput(scripted.fixture.ui).wantsFrame);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // Focus moves once for each time that the script sets the attribute, and not at all if it resets it before the
    // round.
    focusAndEnter("kitTree.set(first, 'focus', '1');", 0);
    focusAndEnter("kitTree.set(second, 'focus', '2'); kitTree.set(second, 'focus', null);", kNone);
    CHECK(prUiFocus(direct.ui) == widgets[0]);
    CHECK(activations[0] == 2);
    CHECK(activations[1] == 1);
    CHECK(scripted.number("clicks") == 211);
    frames(30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A widget that takes no focus, or is disabled, leaves no widget with focus, so Enter activates none.
    focusAndEnter("kitTree.set(note, 'focus', '1');", 3);
    CHECK(prUiFocus(direct.ui) == kPrLayoutNodeNone);
    CHECK(prUiFocus(scripted.fixture.ui) == kPrLayoutNodeNone);
    focusAndEnter("kitTree.set(second, 'focus', '3');", 1);
    CHECK(prUiFocus(scripted.fixture.ui) != kPrLayoutNodeNone);
    focusAndEnter("kitTree.set(locked, 'focus', '1');", 2);
    CHECK(prUiFocus(direct.ui) == kPrLayoutNodeNone);
    CHECK(prUiFocus(scripted.fixture.ui) == kPrLayoutNodeNone);
    CHECK(activations[0] == 2);
    CHECK(activations[1] == 2);
    CHECK(scripted.number("clicks") == 2112);
    frames(30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a script's request for another round is told to the host once") {
    JsKitScripted scripted;
    CHECK(!prJsKitTreeTakeRoundRequest(scripted.tree));
    scripted.run("kitTree.requestRound(); kitTree.requestRound();");
    CHECK(prJsKitTreeTakeRoundRequest(scripted.tree));
    CHECK(!prJsKitTreeTakeRoundRequest(scripted.tree));
}
