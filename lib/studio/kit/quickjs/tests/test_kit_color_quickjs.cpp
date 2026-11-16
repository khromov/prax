// The kinds of Studio Kit's color controls in its QuickJS binding: each declares what the same calls to the kit
// declare.

#include "pr/float.h"
#include "pr/kit_box.h"
#include "pr/kit_color.h"
#include "pr/string.h"

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

    // Clicks at `at`, and runs the frame that routes the click.
    void click(PrVec2 at) {
        scripted.fixture.click(at);
        direct.click(at);
        frame();
    }

    // Presses at `from`, moves to `to` through the point halfway, a frame each, and releases.
    void drag(PrVec2 from, PrVec2 to) {
        push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
        frame();
        PrVec2 halfway{(from.x + to.x) / 2, (from.y + to.y) / 2};
        push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = halfway}});
        frame();
        push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = to}});
        frame();
        push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = to}});
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

    // Returns the point at fractions `x` and `y` of the box of a node of the kit's own tree.
    PrVec2 at(PrLayoutNode node, float x, float y) {
        PrRect box = direct.bounds(node);
        return PrVec2{box.left + (box.right - box.left) * x, box.top + (box.bottom - box.top) * y};
    }
};

// The script that keeps the changes that it hears in `heard`, each as its channels and then its hex value, and gives
// the node the color back in the form that `echo` names, as a component that keeps the color does.
const char* const kHear = R"JS(
    var heard = [];
    var echo = 'none';
    kitTree.onevent = function (node, type, detail) {
        heard.push(type + ':' + detail.values.join(' ') + ':' + detail.text);
        if (echo === 'hex')
            kitTree.set(node, 'value', detail.text);
        if (echo === 'channels')
            kitTree.set(node, 'value', detail.values.join(' '));
    };
)JS";

// Returns what kHear keeps for a change to `color`. A script writes a number as the shortest digits that give it back.
String changeTo(PrColor color) {
    String text = "change:";
    const float channels[4] = {color.r, color.g, color.b, color.a};
    for (uint32_t i = 0; i < 4; ++i) {
        char number[kPrF64TextMaxBytes];
        size_t size = prF64Format(double(channels[i]), number);
        text.append(i == 0 ? "" : " ");
        text.append(PrStringView{number, size});
    }
    char hex[9];
    text.append(":");
    text.append(prKitColorHex(color, hex));
    return text;
}

// Returns child `index` of `node` in a fixture's tree.
PrLayoutNode childAt(KitFixture& f, PrLayoutNode node, uint32_t index) {
    PrLayoutNode at = prLayoutNodeFirstChild(f.tree, node);
    for (uint32_t i = 0; i < index; ++i)
        at = prLayoutNodeNextSibling(f.tree, at);
    return at;
}

// Returns the first node at or under `node`, in the tree's order, whose box is `width` by `height` px, or none.
PrLayoutNode sized(KitFixture& f, PrLayoutNode node, float width, float height) {
    PrRect box = f.bounds(node);
    if (box.right - box.left == width && box.bottom - box.top == height)
        return node;
    for (PrLayoutNode child = prLayoutNodeFirstChild(f.tree, node); child != kPrLayoutNodeNone;
            child = prLayoutNodeNextSibling(f.tree, child)) {
        PrLayoutNode found = sized(f, child, width, height);
        if (found != kPrLayoutNodeNone)
            return found;
    }
    return kPrLayoutNodeNone;
}

// The side of the box of a picker's plane in px, with its border.
constexpr float kPlaneBox = 202;

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("color fields render as the kit's own do, and a color picked in a field's picker reaches the script") {
    Pair pair;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '8'});
        var tint = make('color-field', row, {value: '#FF8000'});
        make('color-field', row, {value: '0.2 0.4 0.8 0.5', size: 'sm'});
        make('color-field', row, {value: '336699', disabled: '', size: 'lg'});
        make('color-field', row, {});
        kitTree.listen(tint, 'change', true);
        echo = 'hex';
    )JS");
    PrColor tint{1, 128.0f / 255, 0, 1};
    PrColor translucent{0.2f, 0.4f, 0.8f, 0.5f};
    PrColor locked{51.0f / 255, 102.0f / 255, 153.0f / 255, 1};
    PrColor unset{};
    PrLayoutNode field = kPrLayoutNodeNone;
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{.direction = PrKitDirection_Row, .gap = 8, .padding = prKitEdgesAll(8)};
        PrLayoutNode row = prKitBox(kit, root, {}, style);
        field = prKitColorField(kit, row, {.color = &tint}, {});
        prKitColorField(kit, row, {.color = &translucent}, {.size = PrKitControlSize_Sm});
        prKitColorField(kit, row, {.color = &locked, .disabled = true}, {.size = PrKitControlSize_Lg});
        prKitColorField(kit, row, {.color = &unset}, {});
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // A click opens the field's picker in a popover, in which a press on the plane picks a color.
    pair.click(pair.direct.center(field));
    pair.frame();
    PrLayoutNode plane = sized(pair.direct, pair.direct.root, kPlaneBox, kPlaneBox);
    REQUIRE(plane != kPrLayoutNodeNone);
    CHECK(pair.alike());
    pair.click(pair.at(plane, 0.3f, 0.4f));
    PrColor picked = tint;
    CHECK(picked.g != 128.0f / 255);
    CHECK(pair.text("heard.join(' ')") == changeTo(picked));

    // The script set the hex value that it heard, which rounds the color that the field holds. The field kept its
    // color, so a press on the strip of hues composes the same color from it in both.
    pair.click(pair.at(prLayoutNodeNextSibling(pair.direct.tree, plane), 0.5f, 0.7f));
    CHECK(tint.g != picked.g);
    CHECK(pair.scripted.number("heard.length") == 2);
    CHECK(pair.text("heard[1]") == changeTo(tint));
    CHECK(pair.settledAlike());

    // Another color from the script replaces the field's.
    pair.scripted.run("kitTree.set(tint, 'value', '#00FF0080');");
    tint = PrColor{0, 1, 0, 128.0f / 255};
    CHECK(pair.settledAlike());
    CHECK(pair.scripted.number("heard.length") == 2);
}

TEST_CASE("color pickers render as the kit's own do in each mode, and their edits reach the script") {
    Pair pair;
    pair.scripted.fixture.display.gamut = PrColorSpace_DisplayP3;
    pair.direct.display.gamut = PrColorSpace_DisplayP3;
    pair.scripted.run(kHear);
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '16', padding: '8', align: 'start'});
        var hsv = make('color-picker', row, {value: '1 0 0'});
        var oklch = make('color-picker', row, {value: '0.2 0.4 0.8', mode: 'oklch', gamut: 'display', opaque: ''});
        var ramp = make('color-picker', row, {value: '#CC4D3380', mode: 'ramp', gamut: 'rec2020'});
        for (const picker of [hsv, oklch, ramp])
            kitTree.listen(picker, 'change', true);
        echo = 'channels';
    )JS");
    PrColor red{1, 0, 0, 1};
    PrColor blue{0.2f, 0.4f, 0.8f, 1};
    PrColor rust{204.0f / 255, 77.0f / 255, 51.0f / 255, 128.0f / 255};
    PrLayoutNode pickers[3] = {};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Start,
            .gap = 16,
            .padding = prKitEdgesAll(8),
        };
        PrLayoutNode row = prKitBox(kit, root, {}, style);
        pickers[0] = prKitColorPicker(kit, row, {.color = &red});
        PrKitColorPicker second{
            .color = &blue,
            .mode = PrKitColorMode_Oklch,
            .gamut = prUiDisplay(pair.direct.ui).gamut,
            .opaque = true,
        };
        pickers[1] = prKitColorPicker(kit, row, second);
        PrKitColorPicker third{.color = &rust, .mode = PrKitColorMode_Ramp, .gamut = PrColorSpace_Rec2020};
        pickers[2] = prKitColorPicker(kit, row, third);
    };
    pair.frame(2);
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.paintedCommands());

    // A picker's second row holds its plane and then its strips, and a ramp's fourth row its tones.
    auto plane = [&](uint32_t picker) { return childAt(pair.direct, childAt(pair.direct, pickers[picker], 1), 0); };

    // A drag on the HSV plane is a change for each move that makes another color.
    pair.drag(pair.at(plane(0), 0.3f, 0.5f), pair.at(plane(0), 0.5f, 0.6f));
    CHECK(red.r == doctest::Approx(0.4).epsilon(0.03));
    CHECK(pair.scripted.number("heard.length") == 3);
    CHECK(pair.text("heard[2]") == changeTo(red));
    CHECK(pair.settledAlike());

    // The most chroma at a lightness of 0.7 is a color that the display's gamut holds and sRGB does not, whose channels
    // the script hears as they are.
    pair.click(pair.at(plane(1), 0.7f, 0.01f));
    bool pastSrgb = false;
    const float channels[] = {blue.r, blue.g, blue.b};
    for (float channel : channels)
        pastSrgb = pastSrgb || channel < -1e-3f || channel > 1 + 1e-3f;
    CHECK(pastSrgb);
    CHECK(pair.text("heard[heard.length - 1]") == changeTo(blue));
    CHECK(pair.settledAlike());

    // A tone of the ramp becomes the color, which keeps its alpha.
    pair.click(pair.direct.center(childAt(pair.direct, childAt(pair.direct, pickers[2], 3), 2)));
    CHECK(rust.a == 128.0f / 255);
    CHECK(rust.r != 204.0f / 255);
    CHECK(pair.text("heard[heard.length - 1]") == changeTo(rust));
    CHECK(pair.settledAlike());

    // The script's colors replace the pickers', in either form.
    pair.scripted.run(R"JS(
        var before = heard.length;
        kitTree.set(hsv, 'value', '#3366CC');
        kitTree.set(oklch, 'value', '-0.25 1.125 5e-1');
        kitTree.set(ramp, 'value', null);
    )JS");
    red = PrColor{51.0f / 255, 102.0f / 255, 204.0f / 255, 1};
    blue = PrColor{-0.25f, 1.125f, 0.5f, 1};
    rust = PrColor{};
    CHECK(pair.settledAlike());
    CHECK(pair.scripted.number("heard.length - before") == 0);
}

TEST_CASE("a color control's attribute refuses a value that does not suit it, and keeps the value that it had") {
    Pair pair;
    pair.scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '16', padding: '8', align: 'start'});
        var field = make('color-field', row, {value: '#336699', size: 'sm'});
        var picker = make('color-picker', row, {value: '0.2 0.4 0.8 0.5', mode: 'oklch', gamut: 'display-p3'});
        var refused = [
            kitTree.set(field, 'value', 'red'), kitTree.set(field, 'value', ''), kitTree.set(field, 'value', '#GGGGGG'),
            kitTree.set(field, 'value', '#12345'), kitTree.set(field, 'value', '1 2'),
            kitTree.set(field, 'value', '1 2 3 4 5'), kitTree.set(field, 'value', '1 2 NaN'),
            kitTree.set(field, 'value', '1 2 1e999'), kitTree.set(field, 'value', 'rgb(1 2 3)'),
            kitTree.set(field, 'size', 'huge'), kitTree.set(field, 'disabled', 'yes'),
            kitTree.set(field, 'mode', 'hsv'), kitTree.set(field, 'color', '#000000'),
            kitTree.set(picker, 'value', 'accent'), kitTree.set(picker, 'mode', 'rgb'),
            kitTree.set(picker, 'gamut', 'oklch'), kitTree.set(picker, 'gamut', 'p3'),
            kitTree.set(picker, 'opaque', '1'), kitTree.set(picker, 'size', 'sm'),
            kitTree.listen(field, 'click', true), kitTree.listen(picker, 'select', true),
        ].filter(Boolean).length;
        var taken = [
            kitTree.set(field, 'disabled', 'true'), kitTree.set(field, 'disabled', null),
            kitTree.set(picker, 'opaque', ''), kitTree.set(picker, 'opaque', 'false'),
            kitTree.set(picker, 'gamut', 'srgb-linear'), kitTree.set(picker, 'gamut', 'display-p3-linear'),
            kitTree.set(picker, 'gamut', 'a98-rgb'), kitTree.set(picker, 'gamut', 'pro-photo-rgb'),
            kitTree.set(picker, 'gamut', null), kitTree.set(picker, 'gamut', 'display-p3'),
            kitTree.listen(field, 'change', true), kitTree.listen(picker, 'change', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(pair.scripted.number("refused") == 0);
    CHECK(pair.scripted.number("taken") == 12);
    PrColor fieldColor{51.0f / 255, 102.0f / 255, 153.0f / 255, 1};
    PrColor pickerColor{0.2f, 0.4f, 0.8f, 0.5f};
    pair.direct.build = [&](PrLayoutNode root) {
        PrKit* kit = pair.direct.kit;
        PrKitBoxStyle style{
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Start,
            .gap = 16,
            .padding = prKitEdgesAll(8),
        };
        PrLayoutNode row = prKitBox(kit, root, {}, style);
        prKitColorField(kit, row, {.color = &fieldColor}, {.size = PrKitControlSize_Sm});
        PrKitColorPicker content{.color = &pickerColor, .mode = PrKitColorMode_Oklch, .gamut = PrColorSpace_DisplayP3};
        prKitColorPicker(kit, row, content);
    };
    pair.frame(2);
    CHECK(pair.alike());
}
