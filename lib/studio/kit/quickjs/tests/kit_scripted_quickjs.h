// What the tests of Studio Kit's QuickJS binding share: a kit whose rounds declare the tree that a script edits, and
// the comparison of what two kits render.

#pragma once

#include "pr/array.h"
#include "pr/kit_quickjs.h"
#include "pr/mem.h"
#include "pr/quickjs_binding.h"

#include "tools/harness/kit_fixture.h"

#include "tools/harness/test.h"

PR_ASSUME_NONNULL_BEGIN

namespace pr_impl {

using namespace harness;
using namespace pr;

// A realm with a tree bound to it, and a kit whose rounds tell the script of themselves, declare the tree and then
// dispatch its events, as a host's do. A test declares the same widgets with the kit's own calls in a second fixture,
// and compares the two.
struct JsKitScripted {
    KitFixture fixture;
    JSRuntime* runtime = jsRuntimeCreate();
    JSContext* ctx = JS_NewContext(runtime);
    PrJsKitTree* tree = prJsKitTreeCreate();

    JsKitScripted() {
        prJsKitTreeBind(tree, ctx);
        fixture.build = [this](PrLayoutNode root) {
            prJsKitTreeDispatchRound(tree, fixture.kit);
            prJsKitTreeBuild(tree, fixture.kit, root);
            prJsKitTreeDispatch(tree);
        };
    }

    ~JsKitScripted() {
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

// A script's function that makes a node of a kind under a parent, with attributes, given as an object of strings, and
// text, and returns the node. It throws for an attribute that the kind does not take, so that a test of a kind fails on
// a misspelled attribute. A test's script begins with it.
constexpr const char* kJsKitMake = R"JS(
    function make(kind, parent, attributes, text) {
        const node = kitTree.create(kind);
        for (const name in attributes) {
            if (!kitTree.set(node, name, attributes[name]))
                throw new Error('<' + kind + '> takes no ' + name + '="' + attributes[name] + '"');
        }
        if (text !== undefined)
            kitTree.setText(node, text);
        kitTree.insert(parent, node, 0);
        return node;
    }
)JS";

// Returns true if the two kits' last frames render the same pixels.
inline bool jsKitRendersAlike(KitFixture& first, KitFixture& second) {
    Array<uint8_t> a;
    Array<uint8_t> b;
    if (!first.render(&a) || !second.render(&b) || a.count() != b.count())
        return false;
    return prMemEquals(a.data(), b.data(), a.count());
}

} // namespace pr_impl

PR_ASSUME_NONNULL_END
