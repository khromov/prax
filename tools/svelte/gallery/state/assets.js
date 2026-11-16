// The asset table of the gallery's model (examples/studio_gallery/gallery_state.cpp): its rows, the order that a
// column sorts them in, and the helpers that edit a selection of them.

// A row of the asset table: what it shows, and the keys it sorts by. A larger `modifiedKey` is more recent.
function asset(name, type, sizeText, modified, sizeBytes, modifiedKey, icon) {
    return { name, type, sizeText, modified, sizeBytes, modifiedKey, icon };
}

// The asset table's rows.
export const assets = [
    asset('player_base.mesh', 'Mesh', '1.2 MB', '12:04', 1258291, 1204, 'cube'),
    asset('env_diffuse.ktx2', 'Texture', '8.4 MB', '11:58', 8808038, 1158, 'image'),
    asset('footsteps_grass_01.ogg', 'Audio', '340 KB', '10:21', 348160, 1021, 'audio'),
    asset('level_01.pr', 'Scene', '42 KB', '09:17', 43008, 917, 'folder'),
    asset('controller.lua', 'Script', '2.1 KB', 'Yesterday', 2150, 0, 'script'),
    asset('water_pbr.material', 'Material', '620 B', 'Yesterday', 620, 0, 'sphere'),
];

// The columns that the asset table is ordered by, each by its index among the table's columns.
export const AssetSort = { name: 0, type: 1, size: 2, modified: 3 };

// Compares two texts by their code units, which are the bytes of the table's ASCII.
function compareText(a, b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

function compareKeys(a, b) {
    return a < b ? -1 : a > b ? 1 : 0;
}

function assetLess(a, b, column, ascending) {
    let order = 0;
    switch (column) {
        case AssetSort.type:
            order = compareText(a.type, b.type);
            break;
        case AssetSort.size:
            order = compareKeys(a.sizeBytes, b.sizeBytes);
            break;
        case AssetSort.modified:
            order = compareKeys(a.modifiedKey, b.modifiedKey);
            break;
        default:
            order = compareText(a.name, b.name);
            break;
    }
    return ascending ? order < 0 : order > 0;
}

// Returns the indices of `rows`, sorted by `column`. Equal keys keep their order.
export function sortAssetOrder(rows, column, ascending) {
    const order = rows.map((_, index) => index);
    // An insertion sort keeps equal rows in their order.
    for (let i = 1; i < order.length; ++i) {
        const current = order[i];
        let at = i;
        while (at > 0 && assetLess(rows[current], rows[order[at - 1]], column, ascending)) {
            order[at] = order[at - 1];
            --at;
        }
        order[at] = current;
    }
    return order;
}

// Returns true if `selection` holds `value`.
export function selectionHolds(selection, value) {
    return selection.includes(value);
}

// Applies a view's edit to a selection of values: `count` rows from `first`, which `op` says how to join, as
// `replace`, `add` or `toggle`. `keys` gives the value of each row, or is null if a row's index is its value.
export function selectionApply(selection, edit, keys) {
    if (edit.op !== 'replace' && edit.op !== 'add' && edit.op !== 'toggle')
        return;
    if (edit.op === 'replace')
        selection.length = 0;
    for (let i = 0; i < edit.count; ++i) {
        const index = edit.first + i;
        const value = keys !== null ? keys[index] : index;
        let held = false;
        for (let j = 0; j < selection.length; ++j) {
            if (selection[j] !== value)
                continue;
            held = true;
            if (edit.op === 'toggle')
                selection.splice(j, 1);
            break;
        }
        if (!held)
            selection.push(value);
    }
}
