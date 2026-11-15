// JavaScript binding of Studio Kit: a tree of widgets that a script edits and each round declares.
//
// Kit's widgets are declared again in every round, while a script changes what it shows only now and then. The binding
// therefore keeps what the script made as a tree of nodes, which the host declares as Kit's widgets in each round that
// it builds. No script runs to declare a round. A widget's input reaches the script as events, which the host
// dispatches between rounds.
//
// A script edits the tree through the object `kitTree` of its realm. A node is a number, and 0 is the tree's root,
// whose children the host declares under a node of its own:
//
//   kitTree.create(kind)                   makes a node of a kind, detached, and returns it
//   kitTree.destroy(node)                  frees a node and the nodes under it
//   kitTree.insert(parent, node, before)   puts a node under a parent, before a child of it, or last if `before` is 0
//   kitTree.remove(node)                   detaches a node from its parent, with the nodes under it
//   kitTree.set(node, name, value)         sets an attribute from a string, or resets it if `value` is null
//   kitTree.setText(node, text)            sets the text that a label, a button, a badge or a block shows
//   kitTree.listen(node, type, listening)  starts or stops queuing a node's events of a type
//   kitTree.onevent(node, type)            the script's function, which dispatching calls with each queued event
//
// `set` and `listen` return false for a name, a value or a type that the node's kind does not take, and change nothing.
// The other functions throw a TypeError for a node that does not exist, and `insert` for a move that would put a node
// under itself.
//
// A tree is used from the thread of its realm, which is the thread that builds the kit's rounds.

#pragma once

#include "pr/base.h"
#include "pr/kit.h"

#include "quickjs.h"

PR_ASSUME_NONNULL_BEGIN

#if defined(__cplusplus)
extern "C" {
#endif

// The nodes that a script made, and the events of the last round that it has yet to hear of.
typedef struct PrJsKitTree PrJsKitTree;

// Creates a tree that holds its root alone.
PrJsKitTree* prJsKitTreeCreate(void);

// Frees the tree. The realm that it is bound to must be gone, or must run no more scripts, since `kitTree` refers to
// the tree.
void prJsKitTreeDestroy(PrJsKitTree* PR_NULLABLE tree);

// Defines `kitTree` on the global object of `ctx`, which edits the tree and hears its events. A tree is bound to one
// realm.
void prJsKitTreeBind(PrJsKitTree* tree, JSContext* ctx);

// Declares the tree's widgets under `parent` in the open round of the kit's context, in the tree's order, and queues
// the events of the round that the script listens for. Each node keeps its identity from round to round, wherever the
// script moves it. Call during a build.
void prJsKitTreeBuild(PrJsKitTree* tree, PrKit* kit, PrLayoutNode parent);

// Calls the script's `kitTree.onevent` with each queued event, in the order that the round queued them, and empties the
// queue. Returns true if it called the script, which may have edited the tree. The host performs a microtask checkpoint
// afterward, as after any script. Call after a build, never during prJsKitTreeBuild.
bool prJsKitTreeDispatch(PrJsKitTree* tree);

#if defined(__cplusplus)
}
#endif

PR_ASSUME_NONNULL_END
