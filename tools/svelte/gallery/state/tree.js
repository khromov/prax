// The scene tree of the gallery's model (examples/studio_gallery/gallery_state.cpp): its seed, and the helpers that
// edit it. A tree is an array of nodes, flattened in preorder, each with its depth.

// A row of the seeded scene tree. `badge` is a status shown in place of the icon, or empty.
function seed(name, meta, badge, depth, icon, badgeTone, open, hidden) {
    return { name, meta, badge, depth, icon, badgeTone, open, hidden };
}

const seeds = [
    seed('World', '', '', 0, 'folder-open', 'muted', true, false),
    seed('Environment', '', '', 1, 'folder', 'muted', true, false),
    seed('Directional Light', 'Light', '', 2, 'light', 'muted', false, false),
    seed('Sky Dome', 'Mesh', '', 2, 'sphere', 'muted', false, false),
    seed('Fog Volume', 'Volume', 'W', 2, 'cube', 'warning', false, true),
    seed('Player', '', '', 1, 'cube', 'muted', true, false),
    seed('MainCamera', 'Camera', '', 2, 'camera', 'muted', false, false),
    seed('PlayerMesh', 'Mesh', '', 2, 'cube', 'muted', false, false),
    seed('Controller', 'Script', '', 2, 'script', 'muted', false, false),
    seed('Level_01', '', '', 1, 'folder', 'muted', false, false),
    seed('Changes', '', '', 0, 'folder-open', 'muted', true, false),
    seed('world.scene', '', 'M', 1, null, 'warning', false, false),
    seed('player.script', '', 'A', 1, null, 'success', false, false),
    seed('old_sky.mat', '', 'D', 1, null, 'danger', false, false),
    seed('level_01.scene', 'was level_1.scene', 'R', 1, null, 'accent', false, false),
    seed('a_very_long_component_name_that_overflows_the_row.scene', '+128 -64', 'M', 1, null, 'warning', false, false),
];

// Returns one past the last node of the subtree at `start`.
function subtreeEnd(nodes, start) {
    let end = start + 1;
    while (end < nodes.length && nodes[end].depth > nodes[start].depth)
        ++end;
    return end;
}

function indexOfKey(nodes, key) {
    for (let i = 0; i < nodes.length; ++i) {
        if (nodes[i].key === key)
            return i;
    }
    return nodes.length;
}

// Finds which nodes have children, and closes the nodes that have none.
function markChildren(nodes) {
    for (let i = 0; i < nodes.length; ++i) {
        nodes[i].hasChildren = i + 1 < nodes.length && nodes[i + 1].depth > nodes[i].depth;
        if (!nodes[i].hasChildren)
            nodes[i].open = false;
    }
}

// Returns the seeded tree's nodes. A node's `key` is its identity, which a move keeps.
export function seedTree() {
    const nodes = seeds.map((from, key) => ({
        name: from.name,
        meta: from.meta,
        badge: from.badge,
        depth: from.depth,
        key,
        icon: from.icon,
        badgeTone: from.badgeTone,
        hasChildren: false,
        open: false,
        hidden: from.hidden,
    }));
    markChildren(nodes);
    for (let i = 0; i < nodes.length; ++i)
        nodes[i].open = nodes[i].hasChildren && seeds[i].open;
    return nodes;
}

// Returns the node of `key`, or null.
export function findTreeNode(nodes, key) {
    const index = indexOfKey(nodes, key);
    return index < nodes.length ? nodes[index] : null;
}

// Moves the node of `key`, with its subtree, relative to the node of `targetKey`: `before` it, `inside` it or `after`
// it. A move onto itself or into its own subtree does nothing.
export function moveTreeNode(nodes, key, targetKey, placement) {
    const start = indexOfKey(nodes, key);
    if (start >= nodes.length || key === targetKey)
        return;
    const end = subtreeEnd(nodes, start);
    for (let i = start; i < end; ++i) {
        if (nodes[i].key === targetKey)
            return;
    }
    const moved = nodes.splice(start, end - start);
    const at = indexOfKey(nodes, targetKey);
    const depth = placement === 'inside' ? nodes[at].depth + 1 : nodes[at].depth;
    const shift = depth - moved[0].depth;
    for (const node of moved)
        node.depth += shift;
    let insertAt = at;
    if (placement === 'inside')
        insertAt = at + 1;
    else if (placement === 'after')
        insertAt = subtreeEnd(nodes, at);
    nodes.splice(insertAt, 0, ...moved);
    markChildren(nodes);
    if (placement === 'inside')
        nodes[at].open = true;
}

// Returns the tree's visible rows, in display order: every node whose ancestors are open.
export function treeOrder(nodes) {
    const rows = [];
    let skipping = false;
    let skipDepth = 0;
    for (const node of nodes) {
        if (skipping && node.depth > skipDepth)
            continue;
        skipping = node.hasChildren && !node.open;
        skipDepth = node.depth;
        rows.push(node);
    }
    return rows;
}
