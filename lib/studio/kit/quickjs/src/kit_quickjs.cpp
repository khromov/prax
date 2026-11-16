// JavaScript binding of Studio Kit: the object that a script edits a tree of widgets through, and what a host does with
// the tree.

#include "pr/kit_quickjs.h"

#include "pr/alloc.h"
#include "pr/assert.h"
#include "pr/count_of.h"
#include "pr/memory.h"
#include "pr/quickjs_binding.h"

#include "studio/kit/quickjs/src/kit_node_quickjs.h"

namespace pr_impl {

namespace {

// A tree, and the realm that it is bound to.
struct JsKitBinding {
    JsKitNodeTree* tree;
    JSContext* PR_NULLABLE ctx;
    bool roundRequested;            // the script asked for the tree to be declared again before the frame is shown
};

JSClassID kitTreeClass;

JsKitNodeTree* PR_NULLABLE treeOf(JSContext* ctx, JSValueConst treeObject) {
    JsKitBinding* binding = static_cast<JsKitBinding*>(JS_GetOpaque2(ctx, treeObject, kitTreeClass));
    return binding != nullptr ? binding->tree : nullptr;
}

// Reads `value` as a node of the tree. Returns false, with a TypeError pending, if it names none.
bool readNode(JSContext* ctx, const JsKitNodeTree* tree, JSValueConst value, JsKitNodeIndex* out) {
    uint32_t index = 0;
    if (JS_ToUint32(ctx, &index, value) < 0)
        return false;
    if (!jsKitTreeHas(tree, index)) {
        JS_ThrowTypeError(ctx, "kitTree: there is no node %u", index);
        return false;
    }
    *out = index;
    return true;
}

// Reads `value` as a node that a script made, which the root is not.
bool readOwnNode(JSContext* ctx, const JsKitNodeTree* tree, JSValueConst value, JsKitNodeIndex* out) {
    if (!readNode(ctx, tree, value, out))
        return false;
    if (*out == 0) {
        JS_ThrowTypeError(ctx, "kitTree: the root stays as it is");
        return false;
    }
    return true;
}

JSValue createMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    PrStringView name = {};
    if (tree == nullptr || argc < 1 || !jsReadString(ctx, argv[0], &name))
        return JS_EXCEPTION;
    const JsKitKind* kind = jsKitKindFrom(name);
    if (kind == nullptr)
        return JS_ThrowTypeError(ctx, "kitTree: there is no kind of node named %.*s", int(name.size), name.data);
    return JS_NewUint32(ctx, jsKitNodeCreate(tree, kind));
}

JSValue destroyMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex node = 0;
    if (tree == nullptr || argc < 1 || !readOwnNode(ctx, tree, argv[0], &node))
        return JS_EXCEPTION;
    jsKitNodeDestroy(tree, node);
    return JS_UNDEFINED;
}

JSValue insertMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex parent = 0;
    JsKitNodeIndex node = 0;
    JsKitNodeIndex before = kJsKitNodeNone;
    if (tree == nullptr || argc < 2 || !readNode(ctx, tree, argv[0], &parent) || !readOwnNode(ctx, tree, argv[1], &node))
        return JS_EXCEPTION;
    uint32_t given = 0;
    if (argc > 2 && JS_ToUint32(ctx, &given, argv[2]) < 0)
        return JS_EXCEPTION;
    if (given != 0 && !readNode(ctx, tree, argv[2], &before))
        return JS_EXCEPTION;
    if (jsKitNodeIsWithin(tree, parent, node))
        return JS_ThrowTypeError(ctx, "kitTree: node %u cannot go under itself", node);
    if (before != kJsKitNodeNone && (before == node || jsKitNodeParent(tree, before) != parent))
        return JS_ThrowTypeError(ctx, "kitTree: node %u is not another child of node %u", before, parent);
    jsKitNodeInsert(tree, parent, node, before);
    return JS_UNDEFINED;
}

JSValue removeMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex node = 0;
    if (tree == nullptr || argc < 1 || !readOwnNode(ctx, tree, argv[0], &node))
        return JS_EXCEPTION;
    jsKitNodeRemove(tree, node);
    return JS_UNDEFINED;
}

JSValue setMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex node = 0;
    PrStringView name = {};
    PrStringView value = {};
    if (tree == nullptr || argc < 2 || !readNode(ctx, tree, argv[0], &node) || !jsReadString(ctx, argv[1], &name))
        return JS_EXCEPTION;
    bool reset = argc < 3 || JS_IsNull(argv[2]) || JS_IsUndefined(argv[2]);
    if (!reset && !jsReadString(ctx, argv[2], &value))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, jsKitNodeSet(tree, node, name, value, reset));
}

JSValue setTextMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex node = 0;
    PrStringView text = {};
    if (tree == nullptr || argc < 2 || !readOwnNode(ctx, tree, argv[0], &node) || !jsReadString(ctx, argv[1], &text))
        return JS_EXCEPTION;
    jsKitNodeSetText(tree, node, text);
    return JS_UNDEFINED;
}

JSValue listenMethod(JSContext* ctx, JSValueConst treeObject, int argc, JSValueConst* argv) {
    JsKitNodeTree* tree = treeOf(ctx, treeObject);
    JsKitNodeIndex node = 0;
    PrStringView type = {};
    if (tree == nullptr || argc < 3 || !readOwnNode(ctx, tree, argv[0], &node) || !jsReadString(ctx, argv[1], &type))
        return JS_EXCEPTION;
    return JS_NewBool(ctx, jsKitNodeListen(tree, node, type, JS_ToBool(ctx, argv[2]) > 0));
}

JSValue requestRoundMethod(JSContext* ctx, JSValueConst treeObject, int, JSValueConst*) {
    JsKitBinding* binding = static_cast<JsKitBinding*>(JS_GetOpaque2(ctx, treeObject, kitTreeClass));
    if (binding == nullptr)
        return JS_EXCEPTION;
    binding->roundRequested = true;
    return JS_UNDEFINED;
}

constexpr JSCFunctionListEntry kTreeFunctions[] = {
    JS_CFUNC_DEF("create", 1, JsCallScoped<createMethod>::call),
    JS_CFUNC_DEF("destroy", 1, JsCallScoped<destroyMethod>::call),
    JS_CFUNC_DEF("insert", 3, JsCallScoped<insertMethod>::call),
    JS_CFUNC_DEF("remove", 1, JsCallScoped<removeMethod>::call),
    JS_CFUNC_DEF("set", 3, JsCallScoped<setMethod>::call),
    JS_CFUNC_DEF("setText", 2, JsCallScoped<setTextMethod>::call),
    JS_CFUNC_DEF("listen", 3, JsCallScoped<listenMethod>::call),
    JS_CFUNC_DEF("requestRound", 0, JsCallScoped<requestRoundMethod>::call),
};

// Returns what an event tells the script beyond its node and its type, as an object.
JSValue detailObject(JSContext* ctx, const JsKitDetail& detail) {
    JSValue object = JS_NewObject(ctx);
    if (detail.valueCount > 0) {
        JS_SetPropertyStr(ctx, object, "value", JS_NewFloat64(ctx, detail.values[0]));
        JSValue values = JS_NewArray(ctx);
        for (uint32_t i = 0; i < detail.valueCount; ++i)
            JS_SetPropertyUint32(ctx, values, i, JS_NewFloat64(ctx, detail.values[i]));
        JS_SetPropertyStr(ctx, object, "values", values);
    }
    if (detail.hasText)
        JS_SetPropertyStr(ctx, object, "text", JS_NewStringLen(ctx, detail.text.data, detail.text.size));
    return object;
}

// Hands an exception that the script's listener threw to the realm's console, which is where a host looks for a
// script's errors.
void reportException(JSContext* ctx) {
    JSValue exception = JS_GetException(ctx);
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue console = JS_GetPropertyStr(ctx, global, "console");
    JSValue error = JS_GetPropertyStr(ctx, console, "error");
    if (JS_IsFunction(ctx, error))
        JS_FreeValue(ctx, JS_Call(ctx, error, console, 1, &exception));
    JS_FreeValue(ctx, error);
    JS_FreeValue(ctx, console);
    JS_FreeValue(ctx, global);
    JS_FreeValue(ctx, exception);
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

// C opaque types
struct PrJsKitTree : JsKitBinding {};

PrJsKitTree* prJsKitTreeCreate(void) {
    PrJsKitTree* tree = allocZeroed<PrJsKitTree>();
    tree->tree = jsKitTreeCreate();
    return tree;
}

void prJsKitTreeDestroy(PrJsKitTree* PR_NULLABLE tree) {
    if (tree == nullptr)
        return;
    jsKitTreeDestroy(tree->tree);
    prMemoryFree(tree);
}

void prJsKitTreeBind(PrJsKitTree* tree, JSContext* ctx) {
    PR_ASSERT(tree->ctx == nullptr);
    tree->ctx = ctx;
    JSClassDef definition = {.class_name = "KitTree"};
    jsRegisterClass(JS_GetRuntime(ctx), &kitTreeClass, definition);
    JSValue object = JS_NewObjectClass(ctx, int(kitTreeClass));
    JS_SetOpaque(object, static_cast<JsKitBinding*>(tree));
    JS_SetPropertyFunctionList(ctx, object, kTreeFunctions, int(countOf(kTreeFunctions)));
    JSValue global = JS_GetGlobalObject(ctx);
    JS_DefinePropertyValueStr(ctx, global, "kitTree", object, JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_FreeValue(ctx, global);
}

void prJsKitTreeBuild(PrJsKitTree* tree, PrKit* kit, PrLayoutNode parent) {
    jsKitTreeDeclare(tree->tree, kit, parent);
}

bool prJsKitTreeDispatch(PrJsKitTree* tree) {
    JSContext* ctx = tree->ctx;
    if (ctx == nullptr || jsKitTreeEventCount(tree->tree) == 0) {
        jsKitTreeClearEvents(tree->tree);
        return false;
    }
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue object = JS_GetPropertyStr(ctx, global, "kitTree");
    JSValue listener = JS_GetPropertyStr(ctx, object, "onevent");
    bool called = false;
    if (JS_IsFunction(ctx, listener)) {
        // The listener may destroy nodes, whose indices name nothing until the queue is empty, so each event still
        // names the node that it was queued for, or none.
        for (uint32_t i = 0; i < jsKitTreeEventCount(tree->tree); ++i) {
            JsKitQueuedEvent event = jsKitTreeEvent(tree->tree, i);
            JSValue arguments[3] = {
                JS_NewUint32(ctx, event.node),
                JS_NewStringLen(ctx, event.type.data, event.type.size),
                detailObject(ctx, event.detail),
            };
            JSValue result = JS_Call(ctx, listener, object, 3, arguments);
            if (JS_IsException(result))
                reportException(ctx);
            JS_FreeValue(ctx, result);
            JS_FreeValue(ctx, arguments[1]);
            JS_FreeValue(ctx, arguments[2]);
            called = true;
        }
    }
    JS_FreeValue(ctx, listener);
    JS_FreeValue(ctx, object);
    JS_FreeValue(ctx, global);
    jsKitTreeClearEvents(tree->tree);
    return called;
}

bool prJsKitTreeIsChanged(const PrJsKitTree* tree) {
    return jsKitTreeIsChanged(tree->tree);
}

bool prJsKitTreeTakeRoundRequest(PrJsKitTree* tree) {
    bool requested = tree->roundRequested;
    tree->roundRequested = false;
    return requested;
}
