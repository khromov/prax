// The Kit backend of the Svelte renderer. A component's elements are Studio Kit's widgets, such as box, label and
// button, with the attributes and the events of the binding's kinds (pr/kit_quickjs.h). This module knows none of them
// by name: an element is a node of the kind that its name gives, its attributes are the node's, and the text nodes
// under it are the node's text.
//
// Svelte builds and edits a tree of its own kinds of node, which this module keeps: fragments, comments and text
// nodes among the elements. An element has a node in the host's tree, `kitTree`, exactly when its parent has one, and
// the root always does, so the host's tree mirrors the elements under the root, which the host declares as widgets in
// each round. An element that Svelte builds off to the side has none until it is put under the root, so what is never
// shown costs the host nothing. An element's text is the text nodes under it, with white space collapsed as HTML
// collapses it, which its widget shows if its kind shows text.

import { flushSync, mount as mountComponent, unmount as unmountComponent } from 'svelte';
import { createRenderer } from 'svelte/renderer';

const tree = globalThis.kitTree;
if (tree === undefined)
    throw new Error('prax-svelte: this bundle is for the kit backend, and its host has no kitTree');

// The attributes that the compiler or a component sets and no widget takes, which are dropped without a warning.
const IGNORED = new Set(['class']);

// A node of any kind. `id` is the element's node in the host's tree, or null while it has none, and `text` is the
// text that the host's node has.
function make(kind, name, data) {
    return {
        kind, name, data, id: null, text: '', parent: null, first: null, last: null, previous: null, next: null,
        attributes: null, listeners: null,
    };
}

// The root, which is the host tree's root: the element that a component is mounted in unless its mount names another.
export const root = make('element', 'root', null);
root.id = 0;

// The elements that have a node in the host's tree, by that node.
const elements = new Map([[0, root]]);

// The elements that were taken from under the root, whose host nodes are detached as they are. Svelte moves a node by
// removing it and inserting it again, so their host nodes are destroyed only once the script has yielded without
// putting them back.
const detached = new Set();

const warned = new Set();
function warn(message) {
    if (!warned.has(message)) {
        warned.add(message);
        console.warn('prax-svelte: ' + message);
    }
}

// Returns the text of an element: its text nodes' data, collapsed.
function textOf(element) {
    let text = '';
    for (let child = element.first; child !== null; child = child.next) {
        if (child.kind === 'text')
            text += child.data;
    }
    return text.replace(/[ \t\r\n]+/g, ' ').trim();
}

// Gives the host's node the element's text, if it changed. The root has no text.
function updateText(element) {
    if (element === null || element.id === null || element === root)
        return;
    const text = textOf(element);
    if (text !== element.text)
        tree.setText(element.id, (element.text = text));
}

function applyAttribute(element, name, value) {
    if (!IGNORED.has(name) && !tree.set(element.id, name, value))
        warn('<' + element.name + '> takes no ' + name + '="' + value + '"');
}

// Gives an element and the elements under it nodes in the host's tree, linked as they are here.
function materialize(element) {
    try {
        element.id = tree.create(element.name);
    } catch {
        warn('there is no <' + element.name + '>, which is declared as a <box>');
        element.id = tree.create('box');
    }
    elements.set(element.id, element);
    element.text = '';
    if (element.attributes !== null) {
        for (const [name, value] of element.attributes)
            applyAttribute(element, name, value);
    }
    if (element.listeners !== null) {
        for (const type of element.listeners.keys())
            listen(element, type, true);
    }
    updateText(element);
    for (let child = element.first; child !== null; child = child.next) {
        if (child.kind === 'element') {
            materialize(child);
            tree.insert(element.id, child.id, 0);
        }
    }
}

// Forgets the host nodes of an element and the elements under it, once the host has destroyed them.
function forget(element) {
    elements.delete(element.id);
    element.id = null;
    for (let child = element.first; child !== null; child = child.next) {
        if (child.kind === 'element' && child.id !== null)
            forget(child);
    }
}

// Puts the host node of an element, whose parent has one, where the element is among its parent's elements.
function connect(element) {
    detached.delete(element);
    if (element.id === null)
        materialize(element);
    let before = element.next;
    while (before !== null && before.kind !== 'element')
        before = before.next;
    tree.insert(element.parent.id, element.id, before === null ? 0 : before.id);
}

// Detaches the host node of an element whose parent has none, to be destroyed unless the element is put back before
// the script yields.
function disconnect(element) {
    tree.remove(element.id);
    if (detached.size === 0)
        queueMicrotask(reap);
    detached.add(element);
}

// Destroys the host nodes of the elements that stayed detached. No detached element is under another, since each was
// detached as it left its parent.
function reap() {
    for (const element of detached) {
        tree.destroy(element.id);
        forget(element);
    }
    detached.clear();
}

function listen(element, type, listening) {
    if (!tree.listen(element.id, type, listening) && listening)
        warn('<' + element.name + '> has no ' + type + ' event');
}

function unlink(node) {
    const parent = node.parent;
    if (node.previous !== null)
        node.previous.next = node.next;
    else
        parent.first = node.next;
    if (node.next !== null)
        node.next.previous = node.previous;
    else
        parent.last = node.previous;
    node.parent = node.previous = node.next = null;
}

function link(parent, node, anchor) {
    node.parent = parent;
    node.next = anchor;
    node.previous = anchor === null ? parent.last : anchor.previous;
    if (node.previous !== null)
        node.previous.next = node;
    else
        parent.first = node;
    if (anchor !== null)
        anchor.previous = node;
    else
        parent.last = node;
}

// Takes a node out of its parent, and its host node out of the host's tree.
function remove(node) {
    const parent = node.parent;
    if (parent === null)
        return;
    unlink(node);
    if (node.kind === 'element' && node.id !== null)
        disconnect(node);
    else if (node.kind === 'text')
        updateText(parent);
}

// Puts a node under a parent before `anchor`, or last. A node that has a parent moves, and a fragment's children go
// in its place.
function insert(parent, node, anchor) {
    if (node.kind === 'fragment') {
        while (node.first !== null)
            insert(parent, node.first, anchor);
        return;
    }
    const from = node.parent;
    if (from !== null)
        unlink(node);
    link(parent, node, anchor);
    if (node.kind === 'element') {
        if (parent.id !== null)
            connect(node);
        else if (node.id !== null)
            disconnect(node);
    } else if (node.kind === 'text') {
        updateText(from);
        updateText(parent);
    }
}

const renderer = createRenderer({
    createFragment: () => make('fragment', null, null),
    createElement: (name) => make('element', name, null),
    createTextNode: (data) => make('text', null, data),
    createComment: (data) => make('comment', null, data),

    nodeType: (node) => node.kind,
    getNodeValue: (node) => (node.kind === 'text' || node.kind === 'comment' ? node.data : null),

    getAttribute: (element, name) => element.attributes?.get(name) ?? null,
    setAttribute(element, key, value) {
        // A boolean is an attribute's presence, as a component's expression for one gives it.
        if (value === false || value === null || value === undefined) {
            renderer.removeAttribute(element, key);
            return;
        }
        const text = value === true ? '' : String(value);
        element.attributes ??= new Map();
        element.attributes.set(key, text);
        if (element.id !== null)
            applyAttribute(element, key, text);
    },
    removeAttribute(element, name) {
        if (element.attributes?.delete(name) && element.id !== null)
            applyAttribute(element, name, null);
    },
    hasAttribute: (element, name) => element.attributes?.has(name) ?? false,

    setText(node, text) {
        if (node.kind === 'element') {
            while (node.first !== null)
                remove(node.first);
            insert(node, make('text', null, String(text)), null);
        } else {
            node.data = String(text);
            if (node.kind === 'text')
                updateText(node.parent);
        }
    },

    getFirstChild: (node) => node.first,
    getLastChild: (node) => node.last,
    getNextSibling: (node) => node.next,
    getParent: (node) => node.parent,

    insert,
    remove,

    addEventListener(target, type, handler) {
        target.listeners ??= new Map();
        let handlers = target.listeners.get(type);
        if (handlers === undefined) {
            target.listeners.set(type, (handlers = new Set()));
            if (target.id !== null)
                listen(target, type, true);
        }
        handlers.add(handler);
    },
    removeEventListener(target, type, handler) {
        const handlers = target.listeners?.get(type);
        if (handlers === undefined || !handlers.delete(handler) || handlers.size > 0)
            return;
        target.listeners.delete(type);
        if (target.id !== null)
            listen(target, type, false);
    },
});

export default renderer;

// The host's events, each of which goes to the listeners of its element alone: a widget's input does not bubble. An
// event has what the host's detail holds for its type: a `value`, `values` and a `text`. The component's state is
// flushed before the host declares its next round, so the round shows what the event changed.
tree.onevent = (id, type, detail) => {
    const target = elements.get(id);
    const handlers = target?.listeners?.get(type);
    if (handlers === undefined)
        return;
    const event = {
        ...detail, type, target, currentTarget: target, defaultPrevented: false,
        preventDefault() {
            event.defaultPrevented = true;
        },
        stopPropagation() {},
    };
    try {
        for (const handler of [...handlers])
            handler.call(target, event);
    } finally {
        flushSync();
    }
};

// A component's styles have nothing to apply to: a widget takes its looks from the kit's theme and its attributes.
export function defineStyles(scope) {
    warn('the styles of a component (' + scope + ') are not applied by the kit backend');
}

// Mounts a component in `target`, the root unless the options name an element, and renders it before returning. The
// result is what unmount takes.
export function mount(component, options = {}) {
    const mounted = mountComponent(component, {
        renderer,
        target: options.target ?? root,
        anchor: options.anchor,
        props: options.props,
    });
    flushSync();
    return mounted;
}

// Removes a mounted component's widgets.
export function unmount(mounted) {
    return unmountComponent(mounted);
}
