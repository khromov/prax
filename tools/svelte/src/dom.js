// The DOM backend of the Svelte renderer. A component's nodes are the engine's own DOM nodes, so each hook is one call
// into the DOM binding, and the engine's cascade, layout and event dispatch do the rest. A component's scoped CSS
// reaches the cascade as a style element.

import { flushSync, mount as mountComponent, unmount as unmountComponent } from 'svelte';
import { createRenderer } from 'svelte/renderer';

// Node.nodeType to the renderer's name for the kind of node.
const KINDS = { 1: 'element', 3: 'text', 8: 'comment', 11: 'fragment' };

// The attributes whose presence is their value. The compiler passes a component's expression for one as a boolean.
const BOOLEAN = new Set([
    'allowfullscreen', 'async', 'autofocus', 'autoplay', 'checked', 'controls', 'default', 'defer', 'disabled',
    'formnovalidate', 'hidden', 'indeterminate', 'inert', 'ismap', 'loop', 'multiple', 'muted', 'nomodule',
    'novalidate', 'open', 'playsinline', 'readonly', 'required', 'reversed', 'selected',
]);

// The attributes that only seed a control's state, which the user's input then replaces, each with the elements that
// it does so for. A component's expression for one has to reach the state itself, as the property of the same name.
const STATEFUL = {
    value: ['input', 'textarea', 'select'],
    checked: ['input'],
    indeterminate: ['input'],
    selected: ['option'],
};

// Sets the property behind a stateful attribute, unless the attribute alone gave the control that state already.
// Leaving the property unset in that case keeps a control that the user has not touched tracking its attribute, as
// one that the HTML parser made does.
function setState(element, key, value) {
    if (!STATEFUL[key].includes(element.localName))
        return;
    const state = BOOLEAN.has(key) ? Boolean(value) : String(value ?? '');
    if (element[key] !== state)
        element[key] = state;
}

const renderer = createRenderer({
    createFragment: () => document.createDocumentFragment(),
    createElement: (name) => document.createElement(name),
    createTextNode: (data) => document.createTextNode(data),
    createComment: (data) => document.createComment(data),

    nodeType: (node) => KINDS[node.nodeType],
    getNodeValue: (node) => node.nodeValue,

    getAttribute: (element, name) => element.getAttribute(name),
    setAttribute(element, key, value) {
        if (BOOLEAN.has(key) && typeof value === 'boolean') {
            // Indeterminate is a property with no attribute behind it.
            if (key !== 'indeterminate')
                element.toggleAttribute(key, value);
        } else {
            element.setAttribute(key, value);
        }
        if (Object.hasOwn(STATEFUL, key))
            setState(element, key, value);
    },
    removeAttribute(element, name) {
        element.removeAttribute(name);
        if (Object.hasOwn(STATEFUL, name))
            setState(element, name, null);
    },
    hasAttribute: (element, name) => element.hasAttribute(name),

    setText(node, text) {
        if (node.nodeType === 1)
            node.textContent = text;
        else
            node.nodeValue = text;
    },

    getFirstChild: (node) => node.firstChild,
    getLastChild: (node) => node.lastChild,
    getNextSibling: (node) => node.nextSibling,
    getParent: (node) => node.parentNode,

    insert: (parent, node, anchor) => parent.insertBefore(node, anchor),
    remove: (node) => node.remove(),

    addEventListener: (target, type, handler, options) => target.addEventListener(type, handler, options),
    removeEventListener: (target, type, handler, options) => target.removeEventListener(type, handler, options),
});

export default renderer;

// The scope classes whose style elements are in the document.
const styled = new Set();

// Adds a component's scoped CSS to the document, once for each scope class. A compiled component calls this as its
// module is evaluated, so the styles of every component of a bundle are in place before the first one mounts.
export function defineStyles(scope, css) {
    if (styled.has(scope))
        return;
    styled.add(scope);
    const style = document.createElement('style');
    style.setAttribute('data-svelte', scope);
    style.textContent = css;
    (document.head ?? document.documentElement).append(style);
}

// Mounts a component into `target`, the document's body unless the options name one, and renders it before
// returning. The result is what unmount takes.
export function mount(component, options = {}) {
    const mounted = mountComponent(component, {
        renderer,
        target: options.target ?? document.body,
        anchor: options.anchor,
        props: options.props,
    });
    flushSync();
    return mounted;
}

// Removes a mounted component from the document.
export function unmount(mounted) {
    return unmountComponent(mounted);
}
