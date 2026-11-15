// Studio Kit's QuickJS binding: the tree that a script edits declares what the same calls to the kit declare, and a
// widget's input reaches the script between rounds.

#include "pr/array.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_quickjs.h"
#include "pr/kit_text.h"
#include "pr/quickjs_binding.h"

#include "tools/harness/kit_fixture.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// A realm with a tree bound to it, and a kit whose rounds declare the tree and then dispatch its events.
struct ScriptedKit {
    KitFixture fixture;
    JSRuntime* runtime = jsRuntimeCreate();
    JSContext* ctx = JS_NewContext(runtime);
    PrJsKitTree* tree = prJsKitTreeCreate();

    ScriptedKit() {
        prJsKitTreeBind(tree, ctx);
        fixture.build = [this](PrLayoutNode root) {
            prJsKitTreeBuild(tree, fixture.kit, root);
            prJsKitTreeDispatch(tree);
        };
    }

    ~ScriptedKit() {
        JS_FreeContext(ctx);
        jsRuntimeDestroy(runtime);
        prJsKitTreeDestroy(tree);
    }

    // Runs `source`, which fails the test if it throws.
    void run(const char* source) {
        JSValue result = JS_Eval(ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue error = JS_GetException(ctx);
            const char* text = JS_ToCString(ctx, error);
            INFO((text ? text : "exception"));
            CHECK(false);
            JS_FreeCString(ctx, text);
            JS_FreeValue(ctx, error);
        }
        JS_FreeValue(ctx, result);
    }

    // Returns the number that the expression `source` evaluates to.
    double number(const char* source) {
        JSValue result = JS_Eval(ctx, source, PrStringView(source).size, "kit", JS_EVAL_TYPE_GLOBAL);
        double value = 0;
        CHECK(JS_ToFloat64(ctx, &value, result) == 0);
        JS_FreeValue(ctx, result);
        return value;
    }
};

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
    function make(kind, parent, attributes, text) {
        const node = kitTree.create(kind);
        for (const name in attributes) {
            if (!kitTree.set(node, name, attributes[name]))
                throw new Error('no attribute ' + name);
        }
        if (text !== undefined)
            kitTree.setText(node, text);
        kitTree.insert(parent, node, 0);
        return node;
    }
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

// Returns true if the two kits' last frames render the same pixels.
bool rendersAlike(KitFixture& first, KitFixture& second) {
    Array<uint8_t> a;
    Array<uint8_t> b;
    if (!first.render(&a) || !second.render(&b) || a.count() != b.count())
        return false;
    return prMemEquals(a.data(), b.data(), a.count());
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a tree that a script makes renders as the same calls to the kit do, before and after a click") {
    ScriptedKit scripted;
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
    CHECK(rendersAlike(scripted.fixture, direct));
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
    CHECK(rendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a node that a script moves, removes or destroys is declared where the tree has it") {
    ScriptedKit scripted;
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
    CHECK(rendersAlike(scripted.fixture, direct));

    // The last label moves before the first, as an insert of an attached node moves it.
    scripted.run("kitTree.insert(column, labels[2], labels[0]);");
    order[0] = "third";
    order[1] = "first";
    order[2] = "second";
    scripted.fixture.frame();
    direct.frame();
    CHECK(rendersAlike(scripted.fixture, direct));

    // A removed node is declared no more, and neither is a destroyed one, whose number names nothing afterward.
    scripted.run("kitTree.remove(labels[1]); kitTree.destroy(labels[0]);");
    count = 1;
    scripted.fixture.frame();
    direct.frame();
    CHECK(rendersAlike(scripted.fixture, direct));
    scripted.run(R"JS(
        var threw = 0;
        try { kitTree.setText(labels[0], 'gone'); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.insert(labels[2], column, 0); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.destroy(0); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
        try { kitTree.create('table'); } catch (error) { threw += error instanceof TypeError ? 1 : 0; }
    )JS");
    CHECK(scripted.number("threw") == 4);
}

TEST_CASE("an attribute takes only the values of its kind, and a value that does not suit it changes nothing") {
    ScriptedKit scripted;
    scripted.run(R"JS(
        var box = kitTree.create('box');
        var label = kitTree.create('label');
        var button = kitTree.create('button');
        var taken = [
            kitTree.set(box, 'padding', '4 8 12'), kitTree.set(box, 'wrap', ''), kitTree.set(box, 'width', '50%'),
            kitTree.set(label, 'wrap', 'nowrap'), kitTree.set(label, 'max-width', 'fill'),
            kitTree.set(button, 'size', 'lg'), kitTree.set(button, 'size', null),
            kitTree.listen(button, 'click', true),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(box, 'padding', '4 8 12 16 20'), kitTree.set(box, 'gap', '-1'), kitTree.set(box, 'role', 'title'),
            kitTree.set(label, 'wrap', ''), kitTree.set(label, 'variant', 'primary'), kitTree.set(label, 'width', '1e3'),
            kitTree.set(button, 'size', 'huge'), kitTree.set(0, 'gap', '4'),
            kitTree.listen(button, 'keydown', true), kitTree.listen(label, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 8);
    CHECK(scripted.number("refused") == 0);
}
