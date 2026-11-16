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
//   kitTree.setText(node, text)            sets the text that the node's widget shows, if its kind shows text
//   kitTree.listen(node, type, listening)  starts or stops queuing a node's events of a type
//   kitTree.onevent(node, type, detail)    the script's function, which dispatching calls with each queued event
//   kitTree.requestRound()                 asks the host to declare the tree again before it shows the frame
//
// A kind is one of the kit's widgets, named as an element is: `box`, `label`, `button`. Each of the kit's headers has a
// source in this layer that defines its kinds, and says what attributes each takes and what events it queues. Every
// kind takes the attributes of PrKitPlace besides its own: `width`, `height`, `min-width`, `min-height`, `max-width`,
// `max-height`, `margin`, `flex`, `align-self` and `weight`. A node under another is declared under the other's widget,
// which is how a box holds its content and a menu its items.
//
// Every node has a key of its own, which keeps its widget's state wherever the script moves it. A node of any kind
// takes `key`, a name, which gives it the key of that name in the tree instead (prUiKey). A widget stays the same
// widget from round to round by its key (pr/layout_frame.h), whichever node declares it. A node that takes the name
// that another node had in the round before therefore continues that node's widget, and the two may be of different
// kinds: a `tree-view` that replaces a `list-view` of the same name is the same scroll container, and shows where the
// list was scrolled to. Two nodes that one round declares with one name share a key, which is a script's error, as it
// is a caller's: layout makes the second without the key, and lib/ui outlines both.
//
// A node of any kind but one whose kind declares no widget of its own, as an overlay's does not, also takes `reveal`,
// and queues `bounds`. Setting `reveal`, to any value, scrolls the containers around the widget in the next round by
// the least that brings it into view (prUiReveal), once; setting it to another value does so again. `bounds` tells
// where the last layout put the widget's box, as `values` of its left, top, right and bottom in px of the viewport, in
// the first round that the script listens and whenever they change.
//
// Such a node also takes `drag-source`, `drag-value` and `drag-threshold`, which make its widget the source of a
// payload (prUiDragStart). `drag-source` is the payload's kind, and `drag-value` is its value. Each is a whole number
// of up to 64 bits, and the kind of the system's content (kPrUiPayloadKindSystem) is refused. The widget is a source
// while its kind is not zero. A press on it picks the payload up once the pointer has moved `drag-threshold` px, which
// is 4 unless the script sets it. A threshold of 0 picks the payload up as the press begins. A press that picked a
// payload up ends without a click. A widget that takes no presses, as a label takes none, picks nothing up. The payload
// drops on a node that takes its kind, such as an `asset-field` or a `drop-zone`, and a `drag-ghost` under the source
// shows beside the pointer while the payload is in flight.
//
// An attribute's value is a string: a keyword, a number, a flag, which is present or `true` when set, or several
// numbers separated by spaces. An event's `detail` is an object that holds what the event tells beyond its type: a
// `value` and `values`, numbers, and a `text`, each only if the event has one.
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

// Returns true if the script edited the tree since it was last declared, so that another round would show something
// different. A host that dispatched a round's events asks for a frame if so.
bool prJsKitTreeIsChanged(const PrJsKitTree* tree);

// Returns true if the script called `kitTree.requestRound` since the last call, and forgets that it did. A script calls
// it as it answers an event whose change other widgets are to show in the frame of the event, as a program that
// declares its own widgets shows a value in every widget that it declares after the one that changed it. A host that
// can declares the tree once more, in a frame whose time stands still, before it shows the frame. A host that cannot
// shows the change in its next frame, as it shows any other.
bool prJsKitTreeTakeRoundRequest(PrJsKitTree* tree);

#if defined(__cplusplus)
}
#endif

PR_ASSUME_NONNULL_END
