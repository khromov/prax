// The kinds of Studio Kit's inputs in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_input.h"
#include "pr/kit_menu.h"
#include "pr/kit_text.h"
#include "pr/span.h"
#include "pr/string.h"
#include "pr/ui.h"
#include "pr/ui_text_edit.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// A tree that a script makes, and a kit that declares the same widgets with its own calls. Both take the same input at
// the same times.
struct Pair {
    JsKitScripted scripted;
    KitFixture direct;

    Pair() { scripted.run(kJsKitMake); }

    // Runs `count` frames of each.
    void frame(uint32_t count = 1) {
        for (uint32_t i = 0; i < count; ++i) {
            scripted.fixture.frame();
            direct.frame();
        }
    }

    void push(PrAppEvent event) {
        scripted.fixture.push(event);
        direct.push(event);
    }
    void down(PrVec2 at) { push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = at}}); }
    void move(PrVec2 at) { push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = at}}); }
    void up(PrVec2 at) { push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = at}}); }
    void key(PrAppKey key, PrAppModifierFlags modifiers = 0) {
        scripted.fixture.key(key, modifiers);
        direct.key(key, modifiers);
    }
    void type(PrStringView text) { push(PrAppEvent{.kind = PrAppEventKind_Text, .input = {.text = text}}); }

    // Clicks at `at`, and runs the frame that routes the click.
    void click(PrVec2 at) {
        scripted.fixture.click(at);
        direct.click(at);
        frame();
    }

    // Presses at `from`, moves by each of `steps` px to the right, a frame each, and releases.
    void drag(PrVec2 from, Span<const float> steps) {
        down(from);
        frame();
        PrVec2 at = from;
        for (float step : steps) {
            at.x += step;
            move(at);
            frame();
        }
        up(at);
        frame();
    }

    // Returns true if the last frames of both rendered the same pixels, and the layout of the script's tree matches a
    // layout from scratch.
    bool alike() { return jsKitRendersAlike(scripted.fixture, direct) && kitVerified(scripted.fixture.tree); }

    // Lets both settle, over frames that route nothing, and compares them.
    bool settledAlike() {
        frame(30);
        return alike();
    }

    // Returns the string that the script's expression `source` evaluates to.
    String text(const char* source) {
        JSValue result = JS_Eval(scripted.ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
        size_t size = 0;
        const char* chars = JS_ToCStringLen(scripted.ctx, &size, result);
        REQUIRE(chars != nullptr);
        String copy(PrStringView{chars, size});
        JS_FreeCString(scripted.ctx, chars);
        JS_FreeValue(scripted.ctx, result);
        return copy;
    }
};

// The script that keeps the events that it hears in `heard`, each as its type and what its detail holds: the numbers of
// `values`, then `text`. A test's script calls `hear` with each node and the types that it listens for on it.
const char* const kHear = R"JS(
    var heard = [];
    function hear(node, types) {
        for (const type of types) {
            if (!kitTree.listen(node, type, true))
                throw new Error('no ' + type + ' event');
        }
    }
    function told(type, detail) {
        const parts = [type];
        if (detail.values !== undefined)
            parts.push(detail.values.join(' '));
        if (detail.text !== undefined)
            parts.push(detail.text);
        return parts.join(':');
    }
    kitTree.onevent = function (node, type, detail) {
        heard.push(told(type, detail));
    };
)JS";

// Writes the text of the control `field` to `owned`, if it changed in the round, and returns the control.
PrLayoutNode taken(PrKit* kit, PrLayoutNode field, String& owned) {
    PrStringView text = {};
    if (prKitTextChanged(kit, field, &text))
        owned.assign(text);
    return field;
}

// Returns child `index` of `node` in a fixture's tree.
PrLayoutNode childAt(KitFixture& f, PrLayoutNode node, uint32_t index) {
    PrLayoutNode at = prLayoutNodeFirstChild(f.tree, node);
    for (uint32_t i = 0; i < index; ++i)
        at = prLayoutNodeNextSibling(f.tree, at);
    return at;
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("number fields that a script makes render as the kit's own do, in each of their attributes") {
    Pair pair;
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '8', padding: '8', align: 'start'});
        make('number-field', column, {value: '1.5'});
        make('number-field', column, {value: '-12.3456', precision: '3', axis: 'X', unit: 'px', 'axis-tone': 'axis-x'});
        make('number-field', column, {value: '7', type: 'i32', size: 'sm', disabled: ''});
        var row = make('box', column, {direction: 'row', gap: '4', width: '400'});
        make('number-field', row, {value: '2e-7', type: 'f64', precision: '8', step: '1e-7', min: '0', max: '1'});
        make('number-field', row, {value: '250', type: 'u8', size: 'lg', unit: 'B'});
        // A field that the script places, as any widget's node is placed.
        make('number-field', row, {value: '0.5', width: '72', flex: 'fixed'});
    )JS");
    float plain = 1.5f;
    float tagged = -12.3456f;
    int32_t whole = 7;
    double tiny = 2e-7;
    uint8_t level = 250;
    float placed = 0.5f;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode column =
            prKitBox(kit, root, {}, {.align = PrKitAlign_Start, .gap = 8, .padding = prKitEdgesAll(8)});
        prKitNumberField(kit, column, {.value = prKitScalarF32(&plain)}, {});
        PrKitNumber axis{.value = prKitScalarF32(&tagged), .axis = "X", .unit = "px", .precision = 3};
        prKitNumberField(kit, column, axis, {.axisTone = PrKitTone_AxisX});
        prKitNumberField(kit, column, {.value = prKitScalarI32(&whole), .disabled = true},
            {.size = PrKitControlSize_Sm});
        PrKitBoxStyle line{.direction = PrKitDirection_Row, .gap = 4, .place = {.width = prKitPx(400)}};
        PrLayoutNode row = prKitBox(kit, column, {}, line);
        PrKitNumber small{
            .value = prKitScalarF64(&tiny),
            .range = {.step = 1e-7, .min = 0, .max = 1, .hasMin = true, .hasMax = true},
            .precision = 8,
        };
        prKitNumberField(kit, row, small, {});
        prKitNumberField(kit, row, {.value = prKitScalarU8(&level), .unit = "B"}, {.size = PrKitControlSize_Lg});
        PrLayoutNode fixed = prKitNumberField(kit, row, {.value = prKitScalarF32(&placed)}, {});
        prKitPlace(kit, fixed, {.width = prKitPx(72), .flex = PrKitFlex_Fixed});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());
}

TEST_CASE("a number field stores its value in each scalar type, as the nearest value that the type holds") {
    Pair pair;
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '4', padding: '8', width: '300'});
        [
            ['f32', '0.1'], ['f64', '-1e300'], ['i8', '300'], ['i16', '-40000'], ['i32', '2.5'], ['i64', '-9e18'],
            ['u8', '-5'], ['u16', '65535.4'], ['u32', '5e9'], ['u64', '1e19'],
        ].forEach(function (field) {
            make('number-field', column, {value: field[1], type: field[0], precision: '4', size: 'xs'});
        });
        // A number that one type cannot hold is kept for a later type that can, whichever attribute is set first.
        var late = make('number-field', column, {type: 'u8', value: '300', size: 'xs'});
        kitTree.set(late, 'type', 'i16');
        var early = make('number-field', column, {value: '-300.75', type: 'i8', size: 'xs'});
        kitTree.set(early, 'type', 'f64');
        var reset = make('number-field', column, {type: 'i8', value: '5', size: 'xs'});
        kitTree.set(reset, 'type', null);
        kitTree.set(reset, 'value', null);
    )JS");
    // What each type stores of its field's number: the number itself, the type's nearest bound, or the nearest whole
    // number.
    float tenth = 0.1f;
    double huge = -1e300;
    int8_t most8 = 127;
    int16_t least16 = -32768;
    int32_t rounded = 3;
    int64_t wide = -9000000000000000000;
    uint8_t least8 = 0;
    uint16_t most16 = 65535;
    uint32_t most32 = 4294967295u;
    uint64_t past = 10000000000000000000ull;
    int16_t late = 300;
    double early = -300.75;
    float reset = 0;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 4, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(300)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        const PrKitScalar typed[] = {
            prKitScalarF32(&tenth), prKitScalarF64(&huge), prKitScalarI8(&most8), prKitScalarI16(&least16),
            prKitScalarI32(&rounded), prKitScalarI64(&wide), prKitScalarU8(&least8), prKitScalarU16(&most16),
            prKitScalarU32(&most32), prKitScalarU64(&past),
        };
        PrKitNumberStyle tiny{.size = PrKitControlSize_Xs};
        for (PrKitScalar value : typed)
            prKitNumberField(kit, column, {.value = value, .precision = 4}, tiny);
        prKitNumberField(kit, column, {.value = prKitScalarI16(&late)}, tiny);
        prKitNumberField(kit, column, {.value = prKitScalarF64(&early)}, tiny);
        prKitNumberField(kit, column, {.value = prKitScalarF32(&reset)}, tiny);
    };
    pair.frame(2);
    CHECK(pair.alike());
}

TEST_CASE("a scrub and a key press on a number field reach the script as changes, and the field keeps them") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '0'});
        var field = make('number-field', row, {value: '1', step: '0.1', min: '-2', max: '4'});
        var count = make('number-field', row, {value: '3', type: 'i32'});
        hear(field, ['change']);
        // The script gives the field the value that it heard, as a component that keeps the value does.
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            kitTree.set(node, 'value', String(detail.value));
        };
    )JS");
    float value = 1;
    int32_t count = 3;
    PrLayoutNode field = kPrLayoutNodeNone;
    PrLayoutNode counter = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {.direction = PrKitDirection_Row, .gap = 0});
        PrKitNumberRange range{.step = 0.1, .min = -2, .max = 4, .hasMin = true, .hasMax = true};
        field = prKitNumberField(kit, row, {.value = prKitScalarF32(&value), .range = range}, {});
        counter = prKitNumberField(kit, row, {.value = prKitScalarI32(&count)}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());

    // A scrub of 20 px adds 20 steps, a move at a time.
    const float right[] = {10, 10};
    pair.drag(pair.direct.center(field), right);
    CHECK(value == doctest::Approx(3.0));
    CHECK(pair.text("heard.join(' ')") == "change:2 change:3");
    CHECK(pair.settledAlike());

    // A scrub past a bound stops at it.
    const float left[] = {-80};
    pair.drag(pair.direct.center(field), left);
    CHECK(value == -2.0f);
    CHECK(pair.scripted.number("heard.length") == 3);
    CHECK(pair.text("heard[2]") == "change:-2");
    const float far[] = {90};
    pair.drag(pair.direct.center(field), far);
    CHECK(value == 4.0f);
    CHECK(pair.text("heard[3]") == "change:4");
    CHECK(pair.settledAlike());

    // The field that the script does not listen to keeps a key's step all the same.
    pair.click(pair.direct.center(counter));
    pair.key(PrAppKey_Up);
    pair.frame();
    CHECK(count == 4);
    CHECK(pair.scripted.number("heard.length") == 4);
    CHECK(pair.settledAlike());
}

TEST_CASE("vector fields render as the kit's own do, and a scrub of a component reaches the script with them all") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '8', padding: '8', width: '420'});
        var uv = make('vector-field', column, {value: '0.25 0.5', step: '0.01', precision: '3'});
        var position = make('vector-field', column, {value: '1 2 3', step: '0.5'});
        make('vector-field', column, {value: '0 0 0 1', type: 'f64', untagged: '', size: 'sm', disabled: ''});
        var cells = make('vector-field', column, {value: '3 4', type: 'i16', min: '0', max: '5'});
        make('vector-field', column, {});
        hear(position, ['change']);
        hear(cells, ['change']);
    )JS");
    float uv[2] = {0.25f, 0.5f};
    float position[3] = {1, 2, 3};
    double orientation[4] = {0, 0, 0, 1};
    int16_t cells[2] = {3, 4};
    float unset[2] = {};
    PrLayoutNode positionField = kPrLayoutNodeNone;
    PrLayoutNode cellsField = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(420)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrKitVector first{.value = prKitScalarF32(uv), .count = 2, .range = {.step = 0.01}, .precision = 3};
        prKitVectorField(kit, column, first, {});
        PrKitVector second{.value = prKitScalarF32(position), .count = 3, .range = {.step = 0.5}};
        positionField = prKitVectorField(kit, column, second, {});
        PrKitVector third{.value = prKitScalarF64(orientation), .count = 4, .untagged = true, .disabled = true};
        prKitVectorField(kit, column, third, {.size = PrKitControlSize_Sm});
        PrKitVector fourth{
            .value = prKitScalarI16(cells),
            .count = 2,
            .range = {.min = 0, .max = 5, .hasMin = true, .hasMax = true},
        };
        cellsField = prKitVectorField(kit, column, fourth, {});
        prKitVectorField(kit, column, {.value = prKitScalarF32(unset), .count = 2}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());

    // The middle component of the position, which has a field of its own.
    PrRect row = pair.direct.bounds(positionField);
    const float steps[] = {4, 4};
    pair.drag(PrVec2{(row.left + row.right) / 2, (row.top + row.bottom) / 2}, steps);
    CHECK(position[1] == 6.0f);
    CHECK(pair.text("heard.join(' ')") == "change:1 4 3 change:1 6 3");
    CHECK(pair.settledAlike());

    // The first of the cells, which stops at the range's bound.
    row = pair.direct.bounds(cellsField);
    const float far[] = {60};
    pair.drag(PrVec2{row.left + (row.right - row.left) / 4, (row.top + row.bottom) / 2}, far);
    CHECK(cells[0] == 5);
    CHECK(pair.text("heard[2]") == "change:5 4");
    CHECK(pair.settledAlike());
}

TEST_CASE("sliders render as the kit's own do, and a press and the keys reach the script as changes") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '12', padding: '16', width: '300'});
        var level = make('slider', column, {value: '2', max: '10'});
        var steps = make('slider', column, {value: '3', type: 'i32', max: '8', size: 'sm', step: '0.25'});
        make('slider', column, {value: '200', min: '20', max: '20000', scale: 'ratio', size: 'lg'});
        make('slider', column, {value: '0.5', disabled: ''});
        // A ratio scale over a range that does not rise from above zero is linear.
        make('slider', column, {value: '5', max: '10', scale: 'ratio'});
        hear(level, ['change']);
        hear(steps, ['change']);
    )JS");
    float level = 2;
    int32_t steps = 3;
    float frequency = 200;
    float locked = 0.5f;
    float unranged = 5;
    PrLayoutNode levelSlider = kPrLayoutNodeNone;
    PrLayoutNode stepsSlider = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 12, .padding = prKitEdgesAll(16), .place = {.width = prKitPx(300)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        levelSlider = prKitSlider(kit, column, {.value = prKitScalarF32(&level), .max = 10}, {});
        PrKitSlider whole{.value = prKitScalarI32(&steps), .max = 8, .step = 0.25f};
        stepsSlider = prKitSlider(kit, column, whole, {.size = PrKitControlSize_Sm});
        PrKitSlider ratio{
            .value = prKitScalarF32(&frequency),
            .min = 20,
            .max = 20000,
            .scale = PrKitSliderScale_Ratio,
        };
        prKitSlider(kit, column, ratio, {.size = PrKitControlSize_Lg});
        prKitSlider(kit, column, {.value = prKitScalarF32(&locked), .disabled = true}, {});
        prKitSlider(kit, column, {.value = prKitScalarF32(&unranged), .max = 10}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());

    // A press takes the value at the pointer, which is a quarter of the way along the track.
    PrRect box = pair.direct.bounds(levelSlider);
    float pad = kPrKitSpace3 / 2;
    float width = box.right - box.left - 2 * pad;
    pair.click(PrVec2{box.left + pad + width / 4, (box.top + box.bottom) / 2});
    CHECK(level == doctest::Approx(2.5).epsilon(0.001));
    CHECK(pair.scripted.number("heard.length") == 1);
    CHECK(pair.scripted.number("Number(heard[0].split(':')[1])") == doctest::Approx(2.5).epsilon(0.001));
    CHECK(pair.settledAlike());

    // The press left focus on the slider, which End moves to the end of its range.
    pair.key(PrAppKey_End);
    pair.frame();
    CHECK(level == 10.0f);
    CHECK(pair.text("heard[1]") == "change:10");

    // A key press moves the whole slider by its step, a quarter of its track.
    pair.click(pair.direct.center(stepsSlider));
    CHECK(steps == 4);
    pair.key(PrAppKey_Right);
    pair.frame();
    CHECK(steps == 6);
    CHECK(pair.text("heard.slice(2).join(' ')") == "change:4 change:6");
    CHECK(pair.settledAlike());
}

TEST_CASE("text fields render as the kit's own do, in each of their attributes") {
    Pair pair;
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '8', padding: '8', width: '260'});
        make('text-field', column, {value: 'Player  one'});
        make('text-field', column, {placeholder: 'Unnamed entity'});
        make('text-field', column, {value: 'level', placeholder: 'Search', icon: 'search', clearable: ''});
        make('text-field', column, {value: 'lib/gpu/metal', mono: '', size: 'sm'});
        make('text-field', column, {value: 'secret', 'input-kind': 'password', size: 'lg'});
        make('text-field', column, {value: 'locked', disabled: '', clearable: ''});
        var row = make('box', column, {direction: 'row', gap: '4'});
        make('label', row, {}, 'Find');
        make('text-field', row, {value: 'in a row', bare: '', 'select-on-focus': '', history: 'one\ntwo'});
    )JS");
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(260)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        prKitTextField(kit, column, {.text = "Player  one"}, {});
        prKitTextField(kit, column, {.text = "", .placeholder = "Unnamed entity"}, {});
        PrKitTextField search{.text = "level", .placeholder = "Search", .icon = PrKitIconKind_Search, .clearable = true};
        prKitTextField(kit, column, search, {});
        prKitTextField(kit, column, {.text = "lib/gpu/metal"}, {.size = PrKitControlSize_Sm, .mono = true});
        PrKitTextField secret{.text = "secret", .inputKind = PrAppTextInputKind_Password};
        prKitTextField(kit, column, secret, {.size = PrKitControlSize_Lg});
        prKitTextField(kit, column, {.text = "locked", .clearable = true, .disabled = true}, {});
        PrLayoutNode row = prKitBox(kit, column, {}, {.direction = PrKitDirection_Row, .gap = 4});
        prKitLabel(kit, row, {.text = "Find"}, {});
        const PrStringView history[] = {"one", "two"};
        PrKitTextField bare{.text = "in a row", .history = history, .historyCount = 2, .selectOnFocus = true};
        prKitTextField(kit, row, bare, {.bare = true});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());
}

TEST_CASE("typing in a text field reaches the script as changes, and Enter as a submit") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '240', gap: '8', padding: '8'});
        var name = make('text-field', column, {value: 'ab', placeholder: 'Name'});
        hear(name, ['change', 'submit']);
        // The script gives the field the text that it heard, as a component that keeps the text does.
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            if (type === 'change')
                kitTree.set(node, 'value', detail.text);
        };
    )JS");
    String text = "ab";
    PrLayoutNode field = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(240)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        field = taken(kit, prKitTextField(kit, column, {.text = text, .placeholder = "Name"}, {}), text);
    };
    pair.frame(2);
    CHECK(pair.alike());

    // A press at the end of the text puts the caret there, and each typed text is a change.
    PrRect box = pair.direct.bounds(field);
    pair.click(PrVec2{box.right - 4, box.top + 4});
    pair.frame();
    pair.type("cd");
    pair.frame();
    CHECK(text == "abcd");
    CHECK(pair.alike());
    // The value that the script set left the caret where the typing put it.
    pair.key(PrAppKey_Left);
    pair.frame();
    pair.type("X");
    pair.frame();
    pair.key(PrAppKey_Backspace);
    pair.frame();
    pair.type(" y");
    pair.frame();
    CHECK(text == "abc yd");
    CHECK(pair.text("heard.join('|')") == "change:abcd|change:abcXd|change:abcd|change:abc yd");
    CHECK(pair.alike());

    pair.key(PrAppKey_Enter);
    pair.frame();
    CHECK(pair.text("heard[4]") == "submit:abc yd");
    CHECK(pair.scripted.number("heard.length") == 5);
    CHECK(pair.settledAlike());
}

TEST_CASE("a text field keeps what is typed with no listener, and a script that sets another value replaces the edit") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '240', gap: '8', padding: '8'});
        var free = make('text-field', column, {});
        var upper = make('text-field', column, {value: 'ID'});
        hear(upper, ['change']);
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            kitTree.set(node, 'value', detail.text.toUpperCase());
        };
    )JS");
    String free;
    String upper = "ID";
    PrLayoutNode freeField = kPrLayoutNodeNone;
    PrLayoutNode upperField = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(240)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        freeField = taken(kit, prKitTextField(kit, column, {.text = free}, {}), free);
        upperField = prKitTextField(kit, column, {.text = upper}, {});
        PrStringView changed = {};
        if (prKitTextChanged(kit, upperField, &changed)) {
            upper.clear();
            for (char c : changed)
                upper.append(c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c);
        }
    };
    pair.frame(2);
    pair.click(pair.direct.center(freeField));
    pair.frame();
    pair.type("kept");
    pair.frame();
    pair.type(" here");
    pair.frame();
    CHECK(free == "kept here");
    CHECK(pair.scripted.number("heard.length") == 0);
    CHECK(pair.alike());

    PrRect box = pair.direct.bounds(upperField);
    pair.click(PrVec2{box.right - 4, box.top + 4});
    pair.frame();
    pair.type("x7");
    pair.frame();
    pair.type("q");
    pair.frame();
    CHECK(upper == "IDX7Q");
    CHECK(pair.text("heard.join('|')") == "change:IDx7|change:IDX7q");
    CHECK(pair.settledAlike());
}

TEST_CASE("a text field's clear control, its recalled history and a focus by a key reach the script as the kit's own") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '240', gap: '8', padding: '8'});
        var search = make('text-field', column, {value: 'level', icon: 'search', clearable: '', 'select-on-focus': ''});
        var command = make('text-field', column, {placeholder: 'Command', history: 'build\nbench', mono: ''});
        hear(search, ['change']);
        hear(command, ['change', 'submit']);
    )JS");
    String search = "level";
    String command;
    const PrStringView history[] = {"build", "bench"};
    PrLayoutNode searchField = kPrLayoutNodeNone;
    PrLayoutNode commandField = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(240)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrKitTextField first{.text = search, .icon = PrKitIconKind_Search, .clearable = true, .selectOnFocus = true};
        searchField = taken(kit, prKitTextField(kit, column, first, {}), search);
        PrKitTextField second{.text = command, .placeholder = "Command", .history = history, .historyCount = 2};
        commandField = taken(kit, prKitTextField(kit, column, second, {.mono = true}), command);
    };
    pair.frame(2);
    CHECK(pair.alike());

    // Tab brings focus to the first field by a key, which selects its text, so typing replaces it.
    pair.key(PrAppKey_Tab);
    pair.frame();
    REQUIRE(prUiFocus(pair.direct.ui) == searchField);
    pair.type("map");
    pair.frame();
    CHECK(search == "map");
    CHECK(pair.alike());

    // The clear control stands at the field's end while the field has text.
    PrRect box = pair.direct.bounds(searchField);
    pair.click(PrVec2{box.right - 10, (box.top + box.bottom) / 2});
    CHECK(search.isEmpty());
    CHECK(pair.text("heard.join('|')") == "change:map|change:");
    CHECK(pair.settledAlike());

    // Up recalls the latest line and then the one before it, and Down past the latest is the empty line.
    pair.click(pair.direct.center(commandField));
    pair.key(PrAppKey_Up);
    pair.frame();
    pair.key(PrAppKey_Up);
    pair.frame();
    CHECK(command == "build");
    CHECK(pair.alike());
    pair.key(PrAppKey_Enter);
    pair.frame();
    pair.key(PrAppKey_Down);
    pair.frame();
    pair.key(PrAppKey_Down);
    pair.frame();
    CHECK(command.isEmpty());
    CHECK(pair.text("heard.slice(2).join('|')") == "change:bench|change:build|submit:build|change:bench|change:");
    CHECK(pair.settledAlike());
}

TEST_CASE("a script that takes a submitted line into a field's history and empties the field does as a console does") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var lines = ['build', 'bench'];
        var command = make('text-field', 0, {placeholder: 'Command', history: lines.join('\n'), mono: ''});
        hear(command, ['change', 'submit']);
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            if (type !== 'submit')
                return;
            lines.push(detail.text);
            kitTree.set(node, 'history', lines.join('\n'));
            kitTree.set(node, 'value', '');
        };
    )JS");
    Array<String> lines({String("build"), String("bench")});
    String command;
    PrLayoutNode field = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrStringView history[8];
        for (uint32_t i = 0; i < lines.count(); ++i)
            history[i] = lines[i];
        PrKitTextField content{
            .text = command,
            .placeholder = "Command",
            .history = history,
            .historyCount = lines.count(),
        };
        field = taken(kit, prKitTextField(kit, root, content, {.mono = true}), command);
        // The kit leaves Enter to its caller, which reads it from the field's events.
        PrUiEvents events = prUiEvents(pair.direct.ui, field);
        for (uint32_t i = 0; i < events.count; ++i) {
            if (events.events[i].kind == PrAppEventKind_KeyDown && events.events[i].keyboard.key == PrAppKey_Enter) {
                lines.push(command);
                command.clear();
            }
        }
    };
    pair.frame(2);
    pair.click(pair.direct.center(field));
    pair.frame();
    pair.type("test");
    pair.frame();
    pair.key(PrAppKey_Enter);
    pair.frame();
    CHECK(command.isEmpty());
    CHECK(lines.count() == 3);
    CHECK(pair.alike());

    // The field kept its focus, and recalls the lines that it has now as the kit's own field does.
    pair.key(PrAppKey_Up);
    pair.frame();
    String recalled("change:");
    recalled.append(command.view());
    CHECK_FALSE(command.isEmpty());
    CHECK(pair.text("heard.join('|')") == String("change:test|submit:test|").append(recalled.view()));
    CHECK(pair.settledAlike());
}

TEST_CASE("text areas render as the kit's own do, and typed lines reach the script as changes") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '260', gap: '8', padding: '8'});
        var notes = make('text-area', column, {value: 'one two', placeholder: 'Notes', rows: '4'});
        make('text-area', column, {value: 'void main() {\n}', mono: '', size: 'sm', rows: '2'});
        make('text-area', column, {placeholder: 'Nothing yet', disabled: ''});
        hear(notes, ['change']);
    )JS");
    String notes = "one two";
    PrLayoutNode notesArea = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(260)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrKitTextArea first{.text = notes, .placeholder = "Notes", .rows = 4};
        notesArea = taken(kit, prKitTextArea(kit, column, first, {}), notes);
        PrKitTextArea second{.text = "void main() {\n}", .rows = 2};
        prKitTextArea(kit, column, second, {.size = PrKitControlSize_Sm, .mono = true});
        prKitTextArea(kit, column, {.text = "", .placeholder = "Nothing yet", .disabled = true}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());

    // A press below the text puts the caret at its end, where Return starts a line.
    PrRect box = pair.direct.bounds(notesArea);
    pair.click(PrVec2{box.right - 4, box.bottom - 4});
    pair.frame();
    pair.type("!");
    pair.frame();
    pair.key(PrAppKey_Enter);
    pair.frame();
    pair.type("three");
    pair.frame();
    CHECK(notes == "one two!\nthree");
    CHECK(pair.text("heard.join('|')") == "change:one two!|change:one two!\n|change:one two!\nthree");
    CHECK(pair.settledAlike());
}

TEST_CASE("a text area shows the script's runs in its styles as the kit shows a buffer's, and is edited across them") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '260', gap: '8', padding: '8'});
        // The runs are set before the text that they style, as a component's attributes may be.
        var styled = make('text-area', column, {runs: '0 0 6 1', value: 'plain strong', rows: '3'});
        make('text-area-style', styled, {});
        make('text-area-style', styled, {role: 'strong', tone: 'accent'});
        make('text-area-style', styled, {role: 'mono', tone: 'success'});
        // A script counts the start of a run in the code units of its string: one for the letter, two for the face.
        var wide = make('text-area', column, {value: 'é😀 code', runs: '4 2', rows: '2', size: 'sm'});
        make('text-area-style', wide, {});
        make('text-area-style', wide, {role: 'strong'});
        make('text-area-style', wide, {role: 'mono', tone: 'warning'});
        hear(styled, ['change']);
        // The script gives the area the text that it heard, which leaves the area's runs and its caret as they are.
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            kitTree.set(node, 'value', detail.text);
        };
    )JS");
    PrTextEditBuffer* styled = prTextEditBufferCreate("plain strong");
    prTextEditBufferSetStyle(styled, 6, 12, 1);
    PrTextEditBuffer* wide = prTextEditBufferCreate("\xc3\xa9\xf0\x9f\x98\x80 code");
    prTextEditBufferSetStyle(wide, 7, prTextEditBufferSize(wide), 2);
    const PrKitTextAreaStyle styles[] = {
        {PrKitTextRole_Body, PrKitTone_Auto},
        {PrKitTextRole_Strong, PrKitTone_Accent},
        {PrKitTextRole_Mono, PrKitTone_Success},
    };
    const PrKitTextAreaStyle wideStyles[] = {
        {PrKitTextRole_Body, PrKitTone_Auto},
        {PrKitTextRole_Strong, PrKitTone_Auto},
        {PrKitTextRole_Mono, PrKitTone_Warning},
    };
    bool plain = false;
    PrLayoutNode styledArea = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(260)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrKitTextArea first{.buffer = styled, .rows = 3, .styles = styles, .styleCount = 3};
        if (plain)
            first = PrKitTextArea{.text = "plain STRONG!", .rows = 3, .styles = styles, .styleCount = 3};
        styledArea = prKitTextArea(kit, column, first, {});
        PrKitTextArea second{.buffer = wide, .rows = 2, .styles = wideStyles, .styleCount = 3};
        prKitTextArea(kit, column, second, {.size = PrKitControlSize_Sm});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // Text typed at the end of a run takes the run's style.
    PrRect box = pair.direct.bounds(styledArea);
    pair.click(PrVec2{box.right - 4, box.bottom - 4});
    pair.frame();
    pair.type("!");
    pair.frame();
    PrStringView text = prTextEditBufferText(styled);
    CHECK(text == "plain strong!");
    CHECK(prTextEditBufferStyleAt(styled, 12) == 1);
    CHECK(pair.text("heard.join('|')") == "change:plain strong!");
    CHECK(pair.alike());

    // The script's new runs replace the styles of the whole text.
    pair.scripted.run("kitTree.set(styled, 'runs', '0 2 5 0 6 1');");
    prTextEditBufferSetStyle(styled, 0, 5, 2);
    prTextEditBufferSetStyle(styled, 5, 6, 0);
    pair.frame(2);
    CHECK(pair.alike());

    // The script's new text keeps the runs, and the caret after what changed stays at the end of the text.
    pair.scripted.run("kitTree.set(styled, 'value', 'plain STRONG!');");
    PrTextEditChange capitals{.start = 6, .end = 12, .text = "STRONG"};
    prTextEditBufferApply(styled, &capitals, 1, PrTextEditOrigin_Program);
    pair.frame(2);
    pair.type("?");
    pair.frame();
    text = prTextEditBufferText(styled);
    CHECK(text == "plain STRONG!?");
    CHECK(pair.text("heard[1]") == "change:plain STRONG!?");
    CHECK(pair.settledAlike());

    // The script's new text may differ from the old within a code point: in the last byte of the first letter and of
    // the face, and then in the first byte of the last letter. The runs style the new text from its start.
    auto retext = [&](PrStringView value) {
        prTextEditBufferSetText(wide, value);
        prTextEditBufferSetStyle(wide, 0, prTextEditBufferSize(wide), 0);
        prTextEditBufferSetStyle(wide, 7, prTextEditBufferSize(wide), 2);
    };
    pair.scripted.run(R"JS(kitTree.set(wide, 'value', 'è😁 codé');)JS");
    retext("\xc3\xa8\xf0\x9f\x98\x81 cod\xc3\xa9");
    pair.frame(2);
    CHECK(pair.alike());
    pair.scripted.run(R"JS(kitTree.set(wide, 'value', 'è😁 codĩ');)JS");
    retext("\xc3\xa8\xf0\x9f\x98\x81 cod\xc4\xa9");
    pair.frame(2);
    CHECK(pair.alike());

    // Without runs the area's text is a value again, which shows in the area's own style.
    pair.scripted.run("kitTree.set(styled, 'value', 'plain STRONG!'); kitTree.set(styled, 'runs', null);");
    plain = true;
    CHECK(pair.settledAlike());
    CHECK(pair.scripted.number("heard.length") == 2);
    prTextEditBufferDestroy(styled);
    prTextEditBufferDestroy(wide);
}

TEST_CASE("a script styles a text area's selection and steps its history as a toolbar of the kit's caller does") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '320', gap: '8', padding: '8'});
        var tools = make('box', column, {direction: 'row', gap: '4'});
        var buttons = ['Strong', 'Code', 'Undo', 'Redo'].map(function (name) {
            const button = make('button', tools, {}, name);
            kitTree.listen(button, 'click', true);
            return button;
        });
        var area = make('text-area', column, {value: 'plain strong code', runs: '0 0', rows: '3'});
        make('text-area-style', area, {});
        make('text-area-style', area, {role: 'strong', tone: 'accent'});
        make('text-area-style', area, {role: 'mono', tone: 'success'});
        hear(area, ['change', 'runs', 'steps']);
        // What each button asks of the area. The number of the ask makes each value another one than the last.
        var asks = 0;
        function ask(button) {
            asks += 1;
            const name = ['style-selection', 'style-selection', 'undo', 'redo'][button];
            kitTree.set(area, name, ['1 ', '2 ', '', ''][button] + asks);
            // The press took focus from the area, which takes it back as the next round ends. The script asks the host
            // to declare the round after that one in the frame of the click, in which the area shows its focus.
            kitTree.set(area, 'focus', String(asks));
            kitTree.requestRound();
        }
        // The script gives the area the text and the runs that it hears, which leaves the area as it is.
        kitTree.onevent = function (node, type, detail) {
            if (node !== area)
                return ask(buttons.indexOf(node));
            heard.push(told(type, detail));
            if (type !== 'steps')
                kitTree.set(area, type === 'change' ? 'value' : 'runs', detail.text);
        };
    )JS");
    PrTextEditBuffer* buffer = prTextEditBufferCreate("plain strong code");
    PrTextEditHistory* history = prTextEditHistoryCreate(buffer);
    const PrKitTextAreaStyle styles[] = {
        {PrKitTextRole_Body, PrKitTone_Auto},
        {PrKitTextRole_Strong, PrKitTone_Accent},
        {PrKitTextRole_Mono, PrKitTone_Success},
    };
    const char* const names[] = {"Strong", "Code", "Undo", "Redo"};
    PrLayoutNode buttons[4] = {};
    PrLayoutNode area = kPrLayoutNodeNone;
    bool plain = false;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(320)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrLayoutNode tools = prKitBox(kit, column, {}, {.direction = PrKitDirection_Row, .gap = 4});
        bool chosen[4] = {};
        bool acted = false;
        for (uint32_t i = 0; i < 4; ++i) {
            buttons[i] = prKitButton(kit, tools, {.text = names[i]}, {});
            chosen[i] = prKitActivated(kit, buttons[i]);
            acted = acted || chosen[i];
        }
        PrKitTextArea content{.buffer = buffer, .rows = 3, .styles = styles, .styleCount = 3};
        if (plain) {
            content.buffer = nullptr;
            content.text = "\xc3\xa9\xf0\x9f\x98\x80plain STRONG code more!";
        }
        area = prKitTextArea(kit, column, content, {});
        // An area that edits a value has no view, and no history to step.
        PrTextEditView* view = prUiTextEditView(kit->ui, area);
        if (view != nullptr && chosen[0])
            prTextEditViewSetStyle(view, 1);
        if (view != nullptr && chosen[1])
            prTextEditViewSetStyle(view, 2);
        if (view != nullptr && chosen[2])
            prTextEditHistoryUndo(history, view);
        if (view != nullptr && chosen[3])
            prTextEditHistoryRedo(history, view);
        if (acted)
            prUiSetFocus(kit->ui, area);
    };
    // Runs a frame of each, and then the round that the script asked for in answer to its frame, in which time stands
    // still, as a host runs it (prJsKitTreeTakeRoundRequest).
    auto step = [&]() {
        pair.frame();
        if (prJsKitTreeTakeRoundRequest(pair.scripted.tree))
            pair.scripted.fixture.frame(0);
    };
    // Clicks a button of both toolbars. Both areas show what the button did, and their focus, as the frame ends.
    auto press = [&](uint32_t button) {
        PrVec2 at = pair.direct.center(buttons[button]);
        pair.scripted.fixture.click(at);
        pair.direct.click(at);
        step();
        CHECK(prUiFocus(pair.direct.ui) == area);
        CHECK(pair.alike());
    };
    auto keys = [&](PrAppKey key, PrAppModifierFlags modifiers, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            pair.key(key, modifiers);
            pair.frame();
        }
    };
    pair.frame(2);
    CHECK(pair.alike());

    // The last word takes a style, as one step of the history, and the area tells its runs and that there is a step to
    // undo.
    PrRect box = pair.direct.bounds(area);
    pair.click(PrVec2{box.right - 4, box.bottom - 4});
    keys(PrAppKey_Left, PrAppModifierFlag_Shift, 4);
    press(1);
    CHECK(prTextEditBufferStyleAt(buffer, 13) == 2);
    CHECK(pair.text("heard.join('|')") == "steps:0 0|runs:0 0 13 2|steps:1 0");

    // An undo that the script resets before the next round is withdrawn, so the step stands.
    pair.scripted.run("kitTree.set(area, 'undo', 'now'); kitTree.set(area, 'undo', null);");
    pair.frame(30);
    CHECK(pair.alike());
    CHECK(!prUiOutput(pair.scripted.fixture.ui).wantsFrame);

    // A redo finds no step to repeat, and changes nothing. The round that looked for one had declared the area's text
    // by then, so it asks for a frame, which would show what a step changed.
    pair.scripted.run("kitTree.set(area, 'redo', 'now');");
    pair.frame();
    CHECK(prUiOutput(pair.scripted.fixture.ui).wantsFrame);
    pair.frame();
    CHECK(!prUiOutput(pair.scripted.fixture.ui).wantsFrame);
    CHECK(pair.alike());

    // Typing goes on in the area, which has focus again. Text typed after every run's start moves none of them.
    keys(PrAppKey_End, 0, 1);
    pair.type(" more");
    pair.frame();
    keys(PrAppKey_Home, PrAppModifierFlag_Super, 1);
    keys(PrAppKey_Right, PrAppModifierFlag_Shift, 5);
    press(0);
    CHECK(pair.text("heard.slice(3).join('|')") == "change:plain strong code more|runs:0 1 5 0 13 2");

    // Each undo reverts one step: the style of the first word, the typing, and the style of the last word. A step that
    // changes the styles alone tells the runs, and one that changes the text alone tells the text.
    pair.scripted.run("heard = [];");
    press(2);
    CHECK(pair.text("heard.join('|')") == "runs:0 0 13 2|steps:1 1");
    press(2);
    CHECK(prTextEditBufferText(buffer) == "plain strong code");
    press(2);
    CHECK(prTextEditBufferStyleRunCount(buffer) == 0);
    CHECK(pair.text("heard.slice(2).join('|')") == "change:plain strong code|runs:0 0|steps:0 1");
    // With no step left, an undo does nothing but return the focus.
    press(2);
    CHECK(pair.scripted.number("heard.length") == 5);

    // Each redo repeats a step, and leaves the caret where the step left it.
    pair.scripted.run("heard = [];");
    press(3);
    press(3);
    CHECK(prTextEditBufferText(buffer) == "plain strong code more");
    CHECK(pair.text("heard.join('|')") == "runs:0 0 13 2|steps:1 1|change:plain strong code more");

    // With nothing selected, the style waits for the text that is typed next at the caret. That edit ends the steps to
    // redo.
    press(0);
    pair.type("!");
    pair.frame();
    CHECK(prTextEditBufferStyleAt(buffer, 22) == 1);
    CHECK(pair.text("heard.slice(3).join('|')") == "change:plain strong code more!|runs:0 0 13 2 22 1|steps:1 0");
    CHECK(pair.alike());

    // A run's start counts the code units of the script's string: one for the letter and two for the face, which are
    // six bytes of the buffer.
    pair.scripted.run("heard = [];");
    keys(PrAppKey_Home, PrAppModifierFlag_Super, 1);
    pair.type("\xc3\xa9\xf0\x9f\x98\x80");
    pair.frame();
    CHECK(prTextEditBufferStyleAt(buffer, 19) == 2);
    CHECK(pair.text("heard.join('|')") == "change:\xc3\xa9\xf0\x9f\x98\x80plain strong code more!|runs:0 0 16 2 25 1");
    CHECK(pair.alike());

    // A text that the script sets is a program's change of the buffer, which empties the history. The runs that the
    // script gave last style it, which are the runs that the area told.
    pair.scripted.run(R"JS(heard = []; kitTree.set(area, 'value', 'é😀plain STRONG code more!');)JS");
    PrTextEditChange capitals{.start = 12, .end = 18, .text = "STRONG"};
    prTextEditBufferApply(buffer, &capitals, 1, PrTextEditOrigin_Program);
    pair.frame(2);
    CHECK(pair.text("heard.join('|')") == "steps:0 0");
    press(2);
    CHECK(prTextEditBufferText(buffer) == "\xc3\xa9\xf0\x9f\x98\x80plain STRONG code more!");
    CHECK(pair.settledAlike());

    // An area with no runs edits a value, with no buffer and no history, so the buttons return its focus and do nothing
    // more.
    pair.scripted.run("kitTree.set(area, 'runs', null);");
    plain = true;
    pair.frame(2);
    press(0);
    press(2);
    CHECK(pair.scripted.number("heard.length") == 1);
    CHECK(pair.settledAlike());
    prTextEditHistoryDestroy(history);
    prTextEditBufferDestroy(buffer);
}

TEST_CASE("a suggest field opens the script's suggestions as the kit's own does, and a chosen one is a change") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {width: '260', gap: '8', padding: '8'});
        var areas = ['lib/gpu', 'lib/geom', 'lib/layout', 'lib/studio/kit', 'tools/svelte'];
        var area = make('suggest-field', column, {
            value: 'lib/g', placeholder: 'Area path', icon: 'search', mono: '', suggestions: areas.join('\n'),
        });
        make('suggest-field', column, {placeholder: 'No suggestions', size: 'sm'});
        hear(area, ['change', 'submit']);
        // The script offers the areas that begin with the field's text, once the text has changed.
        kitTree.onevent = function (node, type, detail) {
            heard.push(told(type, detail));
            if (type !== 'change')
                return;
            const matches = areas.filter(function (name) { return name.startsWith(detail.text); });
            kitTree.set(node, 'suggestions', matches.join('\n'));
        };
    )JS");
    String text = "lib/g";
    String none;
    const PrStringView areas[] = {"lib/gpu", "lib/geom", "lib/layout", "lib/studio/kit", "tools/svelte"};
    bool changed = false;
    PrLayoutNode field = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(260)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        PrStringView matches[countOf(areas)];
        uint32_t count = 0;
        for (PrStringView name : areas) {
            bool begins = name.size >= text.count() && PrStringView{name.data, text.count()} == text.view();
            if (!changed || begins)
                matches[count++] = name;
        }
        PrKitSuggestField content{
            .field = {.text = text, .placeholder = "Area path", .icon = PrKitIconKind_Search},
            .suggestions = matches,
            .count = count,
        };
        field = prKitSuggestField(kit, column, content, {.mono = true});
        PrStringView now = {};
        if (prKitTextChanged(kit, field, &now)) {
            text.assign(now);
            changed = true;
        }
        PrKitSuggestField empty{.field = {.text = none, .placeholder = "No suggestions"}};
        taken(kit, prKitSuggestField(kit, column, empty, {.size = PrKitControlSize_Sm}), none);
    };
    pair.frame(2);
    CHECK(pair.alike());

    // The list opens as focus comes to the field, with every area.
    pair.click(pair.direct.center(field));
    pair.frame();
    REQUIRE(prKitIsOpen(pair.direct.kit, field));
    CHECK(pair.alike());
    pair.type("e");
    pair.frame();
    CHECK(text == "lib/ge");
    CHECK(pair.alike());

    // The one area that still matches is the list's first row, half a row below the field.
    PrRect box = pair.direct.bounds(field);
    pair.click(PrVec2{(box.left + box.right) / 2, box.bottom + 2 + kPrKitHeightMd * 0.5f});
    CHECK(text == "lib/geom");
    CHECK_FALSE(prKitIsOpen(pair.direct.kit, field));
    CHECK(pair.text("heard.join('|')") == "change:lib/ge|change:lib/geom");
    CHECK(pair.settledAlike());

    // The choice left focus in the field, where Enter submits the chosen text.
    pair.key(PrAppKey_Enter);
    pair.frame();
    CHECK(pair.text("heard.slice(2).join('|')") == "submit:lib/geom");
    CHECK(pair.settledAlike());
}

TEST_CASE("an inspector's groups and rows hold the script's controls, and a header's activation is a change") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var transform = make('inspector-group', 0, {open: '', badge: 'CORE', 'badge-variant': 'accent'}, 'Transform');
        var position = make('inspector-row', transform, {}, 'Position');
        make('vector-field', position, {value: '1 2 3'});
        var mass = make('inspector-row', transform, {mono: '', height: '40'}, 'mass_kg');
        make('number-field', mass, {value: '1', unit: 'kg', precision: '1'});
        var mesh = make('inspector-group', 0, {badge: 'OFF', 'badge-variant': 'warning'}, 'Mesh');
        var lod = make('inspector-row', mesh, {}, 'Lod');
        make('slider', lod, {value: '0.5'});
        hear(transform, ['change']);
        hear(mesh, ['change']);
    )JS");
    bool transformOpen = true;
    bool meshOpen = false;
    float position[3] = {1, 2, 3};
    float mass = 1;
    float lod = 0.5f;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitInspectorGroup first{
            .name = "Transform",
            .badge = "CORE",
            .badgeVariant = PrKitBadgeVariant_Accent,
            .open = &transformOpen,
        };
        PrLayoutNode body = prKitInspectorGroup(kit, root, first);
        if (body != kPrLayoutNodeNone) {
            PrLayoutNode row = prKitInspectorRow(kit, body, {.label = "Position"}, {});
            prKitVectorField(kit, row, {.value = prKitScalarF32(position), .count = 3}, {});
            // A row's placement is its controls', which is the node that the kit returns for the row.
            PrLayoutNode massRow = prKitInspectorRow(kit, body, {.label = "mass_kg"}, {.mono = true});
            prKitPlace(kit, massRow, {.height = prKitPx(40)});
            prKitNumberField(kit, massRow, {.value = prKitScalarF32(&mass), .unit = "kg", .precision = 1}, {});
        }
        PrKitInspectorGroup second{
            .name = "Mesh",
            .badge = "OFF",
            .badgeVariant = PrKitBadgeVariant_Warning,
            .open = &meshOpen,
        };
        PrLayoutNode meshBody = prKitInspectorGroup(kit, root, second);
        if (meshBody != kPrLayoutNodeNone)
            prKitSlider(kit, prKitInspectorRow(kit, meshBody, {.label = "Lod"}, {}), {.value = prKitScalarF32(&lod)}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // A group's frame is a child of the root, and its header is the frame's first child.
    auto header = [&](uint32_t group) { return childAt(pair.direct, childAt(pair.direct, pair.direct.root, group), 0); };
    pair.click(pair.direct.center(header(0)));
    CHECK_FALSE(transformOpen);
    CHECK(pair.text("heard.join(' ')") == "change:0");
    CHECK(pair.settledAlike());

    // The closed group's rows are declared once it opens.
    pair.click(pair.direct.center(header(1)));
    CHECK(meshOpen);
    CHECK(pair.text("heard.join(' ')") == "change:0 change:1");
    CHECK(pair.settledAlike());

    // The script opens and closes a group by its attribute, which tells it of no change.
    pair.scripted.run("kitTree.set(transform, 'open', 'true'); kitTree.set(mesh, 'open', null);");
    transformOpen = true;
    meshOpen = false;
    CHECK(pair.settledAlike());
    CHECK(pair.scripted.number("heard.length") == 2);
}

TEST_CASE("asset fields render as the kit's own do, and their buttons and a dropped payload reach the script") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '8', padding: '8', width: '320'});
        var mesh = make('asset-field', column, {icon: 'cube', kind: '7'}, 'player.mesh');
        make('asset-field', column, {tint: '#3366CC', kind: '7', missing: ''}, 'lost.mat');
        var none = make('asset-field', column, {empty: '', kind: '18446744073709551614'}, 'None');
        make('asset-field', column, {icon: 'sphere', tint: '0.2 0.4 0.8 0.5', disabled: ''}, 'locked.mat');
        hear(mesh, ['browse', 'locate', 'clear', 'drop']);
        hear(none, ['browse', 'drop']);
    )JS");
    // A payload's value and a kind past the whole numbers that a script's number holds.
    const uint64_t kWideValue = 9007199254740993ull;
    const uint64_t kWideKind = 18446744073709551614ull;
    uint64_t kind = 7;
    uint64_t value = kWideValue;
    PrLayoutNode source = kPrLayoutNodeNone;
    PrLayoutNode meshField = kPrLayoutNodeNone;
    PrLayoutNode noneField = kPrLayoutNodeNone;
    // The button that both drag a payload from, which is the kit's own in each, before the fields.
    auto declareSource = [&](KitFixture& f, PrLayoutNode root) {
        PrLayoutNode button = prKitButton(f.kit, root, {.text = "cube.mesh"}, {});
        if (prUiIsPressed(f.ui, button, 0))
            prUiDragStart(f.ui, button, 4, kind, value, 0);
        return button;
    };
    pair.scripted.fixture.build = [&](PrLayoutNode root) {
        declareSource(pair.scripted.fixture, root);
        prJsKitTreeBuild(pair.scripted.tree, pair.scripted.fixture.kit, root);
        prJsKitTreeDispatch(pair.scripted.tree);
    };
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        source = declareSource(pair.direct, root);
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(320)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        meshField = prKitAssetField(kit, column, {.name = "player.mesh", .icon = PrKitIconKind_Cube, .kind = 7});
        PrColor blue{51.0f / 255, 102.0f / 255, 204.0f / 255, 1};
        prKitAssetField(kit, column, {.name = "lost.mat", .tint = blue, .kind = 7, .missing = true});
        noneField = prKitAssetField(kit, column, {.name = "None", .kind = kWideKind, .empty = true});
        PrKitAssetField locked{
            .name = "locked.mat",
            .icon = PrKitIconKind_Sphere,
            .tint = PrColor{0.2f, 0.4f, 0.8f, 0.5f},
            .disabled = true,
        };
        prKitAssetField(kit, column, locked);
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // The buttons of a field's gutter are the children of its last child: browse, locate and clear.
    auto gutter = [&](PrLayoutNode field, uint32_t button) {
        return childAt(pair.direct, prLayoutNodeLastChild(pair.direct.tree, field), button);
    };
    for (uint32_t button = 0; button < 3; ++button)
        pair.click(pair.direct.center(gutter(meshField, button)));
    pair.click(pair.direct.center(gutter(noneField, 0)));
    CHECK(pair.text("heard.join(' ')") == "browse locate clear browse");
    CHECK(pair.settledAlike());

    // A payload of the field's kind dropped on it, which a field of another kind passes by.
    auto carry = [&](PrLayoutNode field) {
        PrVec2 to = pair.direct.center(field);
        pair.down(pair.direct.center(source));
        pair.frame();
        pair.move(to);
        pair.frame(2);
        pair.up(to);
        pair.frame();
    };
    carry(meshField);
    carry(noneField);
    CHECK(pair.text("heard.slice(4).join(' ')") == "drop:9007199254740992:9007199254740993");
    kind = kWideKind;
    value = 5;
    carry(noneField);
    CHECK(pair.text("heard.slice(5).join(' ')") == "drop:5:5");
    CHECK(pair.settledAlike());
}

TEST_CASE("an input's attribute refuses a value that does not suit it, and keeps the value that it had") {
    Pair pair;
    pair.scripted.run(R"JS(
        var column = make('box', 0, {gap: '8', padding: '8', width: '320'});
        var number = make('number-field', column, {value: '2.5', type: 'f64', precision: '1', unit: 'm'});
        var vector = make('vector-field', column, {value: '1 2 3'});
        var field = make('text-field', column, {value: 'kept', icon: 'search'});
        var suggest = make('suggest-field', column, {value: 'also kept'});
        var area = make('text-area', column, {value: 'two\nlines', rows: '2'});
        var look = make('text-area-style', area, {role: 'mono', tone: 'accent'});
        var slider = make('slider', column, {value: '0.25', scale: 'linear'});
        var group = make('inspector-group', column, {open: '', badge: 'B', 'badge-variant': 'success'}, 'Group');
        var row = make('inspector-row', group, {mono: ''}, 'row');
        var asset = make('asset-field', row, {icon: 'cube', tint: '#336699', kind: '3'}, 'asset');
        var refused = [
            kitTree.set(number, 'value', ''), kitTree.set(number, 'value', 'NaN'), kitTree.set(number, 'value', '1 2'),
            kitTree.set(number, 'value', 'Infinity'), kitTree.set(number, 'value', '1e999'),
            kitTree.set(number, 'value', '0x10'), kitTree.set(number, 'value', '1,5'),
            kitTree.set(number, 'type', 'float'), kitTree.set(number, 'step', '-1'), kitTree.set(number, 'min', 'low'),
            kitTree.set(number, 'max', ''), kitTree.set(number, 'precision', '256'),
            kitTree.set(number, 'precision', '1.5'), kitTree.set(number, 'axis-tone', 'loud'),
            kitTree.set(number, 'size', 'huge'), kitTree.set(number, 'disabled', 'yes'),
            kitTree.set(number, 'untagged', ''), kitTree.set(number, 'placeholder', 'none'),
            kitTree.set(vector, 'value', '1'), kitTree.set(vector, 'value', '1 2 3 4 5'),
            kitTree.set(vector, 'value', '1 x'), kitTree.set(vector, 'axis', 'X'),
            kitTree.set(vector, 'axis-tone', 'accent'), kitTree.set(vector, 'untagged', '0'),
            kitTree.set(field, 'icon', 'nope'), kitTree.set(field, 'input-kind', 'secret'),
            kitTree.set(field, 'clearable', 'yes'), kitTree.set(field, 'size', 'huge'),
            kitTree.set(field, 'suggestions', 'a\nb'), kitTree.set(field, 'rows', '2'),
            kitTree.set(suggest, 'icon', ''), kitTree.set(suggest, 'select-on-focus', '1'),
            kitTree.set(area, 'rows', '-1'), kitTree.set(area, 'rows', '2.5'), kitTree.set(area, 'bare', ''),
            kitTree.set(area, 'history', 'a'), kitTree.set(area, 'icon', 'search'),
            kitTree.set(area, 'runs', '1'), kitTree.set(area, 'runs', '0 0 0 1'), kitTree.set(area, 'runs', '5 1 3 2'),
            kitTree.set(area, 'runs', '0 x'), kitTree.set(area, 'runs', '-1 0'), kitTree.set(area, 'runs', '0 1.5'),
            kitTree.set(area, 'style-selection', ''), kitTree.set(area, 'style-selection', 'strong'),
            kitTree.set(area, 'style-selection', '-1'), kitTree.set(area, 'style-selection', '1.5'),
            kitTree.set(area, 'style-selection', '1 x'), kitTree.set(area, 'style-selection', '1 2 3'),
            kitTree.set(field, 'undo', '1'), kitTree.set(field, 'style-selection', '1'),
            kitTree.set(look, 'role', 'loud'), kitTree.set(look, 'tone', 'bright'), kitTree.set(look, 'value', 'a'),
            kitTree.set(slider, 'value', 'half'), kitTree.set(slider, 'step', '-0.1'),
            kitTree.set(slider, 'scale', 'log'), kitTree.set(slider, 'min', ''), kitTree.set(slider, 'precision', '2'),
            kitTree.set(group, 'open', 'yes'), kitTree.set(group, 'badge-variant', 'primary'),
            kitTree.set(group, 'name', 'Other'), kitTree.set(row, 'mono', 'x'), kitTree.set(row, 'label', 'other'),
            kitTree.set(asset, 'icon', 'cubes'), kitTree.set(asset, 'tint', 'red'), kitTree.set(asset, 'tint', '#12345'),
            kitTree.set(asset, 'tint', '1 2'), kitTree.set(asset, 'tint', '1 2 3 4 5'), kitTree.set(asset, 'tint', '369'),
            kitTree.set(asset, 'kind', '-1'), kitTree.set(asset, 'kind', '7.5'), kitTree.set(asset, 'kind', ''),
            kitTree.set(asset, 'missing', '2'), kitTree.set(asset, 'value', '1'),
            kitTree.listen(number, 'submit', true), kitTree.listen(area, 'submit', true),
            kitTree.listen(field, 'runs', true), kitTree.listen(field, 'steps', true),
            kitTree.listen(row, 'change', true), kitTree.listen(asset, 'change', true),
            kitTree.listen(field, 'drop', true),
        ].filter(Boolean).length;
        // Each form of a flag, and a reset of an attribute to its default.
        var taken = [
            kitTree.set(number, 'disabled', 'true'), kitTree.set(number, 'disabled', 'false'),
            kitTree.set(number, 'max', '10'), kitTree.set(number, 'max', null),
            kitTree.set(field, 'placeholder', 'unused'), kitTree.set(field, 'placeholder', null),
            kitTree.set(suggest, 'suggestions', 'a\nb'), kitTree.set(suggest, 'suggestions', null),
            kitTree.set(area, 'placeholder', ''), kitTree.set(slider, 'step', '1e-2'),
            kitTree.set(group, 'badge', 'A'), kitTree.set(group, 'badge', 'B'),
            kitTree.set(asset, 'kind', '3'), kitTree.set(asset, 'tint', '51e-2 0 0'),
            kitTree.set(asset, 'tint', '336699'),
            // What a text area does once, which an area with no runs has no buffer to do to.
            kitTree.set(area, 'style-selection', '1'), kitTree.set(area, 'style-selection', '1 2'),
            kitTree.set(area, 'style-selection', null), kitTree.set(area, 'undo', ''),
            kitTree.set(area, 'redo', 'again'), kitTree.set(area, 'redo', null),
            kitTree.listen(area, 'runs', true), kitTree.listen(area, 'steps', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(pair.scripted.number("refused") == 0);
    CHECK(pair.scripted.number("taken") == 23);
    double number = 2.5;
    float vector[3] = {1, 2, 3};
    float slider = 0.25f;
    bool open = true;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.gap = 8, .padding = prKitEdgesAll(8), .place = {.width = prKitPx(320)}};
        PrLayoutNode column = prKitBox(kit, root, {}, style);
        prKitNumberField(kit, column, {.value = prKitScalarF64(&number), .unit = "m", .precision = 1}, {});
        prKitVectorField(kit, column, {.value = prKitScalarF32(vector), .count = 3}, {});
        prKitTextField(kit, column, {.text = "kept", .icon = PrKitIconKind_Search}, {});
        prKitSuggestField(kit, column, {.field = {.text = "also kept"}}, {});
        prKitTextArea(kit, column, {.text = "two\nlines", .rows = 2}, {});
        prKitSlider(kit, column, {.value = prKitScalarF32(&slider), .step = 0.01f}, {});
        PrKitInspectorGroup group{
            .name = "Group",
            .badge = "B",
            .badgeVariant = PrKitBadgeVariant_Success,
            .open = &open,
        };
        PrLayoutNode row = prKitInspectorRow(kit, prKitInspectorGroup(kit, column, group), {.label = "row"}, {.mono = true});
        PrColor tint{51.0f / 255, 102.0f / 255, 153.0f / 255, 1};
        prKitAssetField(kit, row, {.name = "asset", .icon = PrKitIconKind_Cube, .tint = tint, .kind = 3});
    };
    pair.frame(2);
    CHECK(pair.alike());
}
