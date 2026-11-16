// The collections section's model (examples/studio_gallery/gallery_state.cpp): its five thousand generated assets, the
// keys of its tree's rows, and how a view's edit applies to a selection.

// The collection's groups, and the assets in each.
export const groupCount = 50;
export const groupSize = 100;

// The kinds of the generated assets.
const assetKinds = [
    { type: 'Mesh', extension: 'mesh', icon: 'cube' },
    { type: 'Texture', extension: 'ktx2', icon: 'image' },
    { type: 'Material', extension: 'material', icon: 'sphere' },
    { type: 'Script', extension: 'lua', icon: 'script' },
    { type: 'Audio', extension: 'ogg', icon: 'audio' },
    { type: 'Camera', extension: 'camera', icon: 'camera' },
    { type: 'Light', extension: 'light', icon: 'light' },
];

// The words of the generated assets' names.
const assetQualifiers = [
    'rusty', 'oak', 'granite', 'coastal', 'ruined', 'gilded', 'frozen', 'scorched', 'hollow', 'salvaged', 'painted',
    'weathered', 'brass', 'crimson', 'quiet', 'distant',
];
const assetNouns = [
    'crate', 'bulkhead', 'banner', 'lantern', 'girder', 'console', 'statue', 'barrel', 'hatch', 'railing', 'terminal',
    'pillar', 'canopy', 'beacon', 'bridge', 'workbench',
];

const MASK_64 = (1n << 64n) - 1n;
const MASK_32 = (1n << 32n) - 1n;

// Returns a generator of the sequence that core's PrRandom draws from `seed` (pr/random.h), which is what makes the
// assets the example's: wyrand, and Lemire's bounded draws over it.
function random(seed) {
    let state = seed;
    function u64() {
        state = (state + 0xa0761d6478bd642fn) & MASK_64;
        const product = state * (state ^ 0xe7037ed1a0b428dbn);
        return (product & MASK_64) ^ (product >> 64n);
    }
    return {
        u64,
        u32Below(bound) {
            const range = BigInt(bound);
            let product = (u64() >> 32n) * range;
            if ((product & MASK_32) < range) {
                const threshold = ((1n << 32n) - range) % range;
                while ((product & MASK_32) < threshold)
                    product = (u64() >> 32n) * range;
            }
            return Number(product >> 32n);
        },
        u64Below(bound) {
            let product = u64() * bound;
            if ((product & MASK_64) < bound) {
                const threshold = ((1n << 64n) - bound) % bound;
                while ((product & MASK_64) < threshold)
                    product = u64() * bound;
            }
            return product >> 64n;
        },
    };
}

// Returns `numerator / denominator` rounded to a whole number, with a tie going to the even one, as core's format
// rounds the digits that it prints.
function roundedQuotient(numerator, denominator) {
    const quotient = Math.floor(numerator / denominator);
    const rest = numerator - quotient * denominator;
    if (rest * 2 > denominator || (rest * 2 === denominator && quotient % 2 === 1))
        return quotient + 1;
    return quotient;
}

// Returns a byte count in the largest unit that keeps a whole part.
function sizeLabel(bytes) {
    if (bytes >= 1024 * 1024) {
        const tenths = roundedQuotient(bytes * 10, 1024 * 1024);
        return Math.floor(tenths / 10) + '.' + (tenths % 10) + ' MB';
    }
    if (bytes >= 1024)
        return roundedQuotient(bytes, 1024) + ' KB';
    return bytes + ' B';
}

// Returns the collection's assets. An asset's `tint` is its color as the channels that a color attribute takes, each
// the single-precision number that the example computes.
export function seedCollections() {
    const draw = random(0x5eed1e55c0ffee11n);
    const assets = [];
    for (let i = 0; i < groupCount * groupSize; ++i) {
        const kind = assetKinds[draw.u32Below(assetKinds.length)];
        const qualifier = assetQualifiers[draw.u32Below(assetQualifiers.length)];
        const noun = assetNouns[draw.u32Below(assetNouns.length)];
        const number = String(draw.u32Below(100)).padStart(2, '0');
        // The magnitude is chosen first, so that the sizes spread across their units.
        const magnitude = 9 + draw.u32Below(16);
        const sizeBytes = 64 + Number(draw.u64Below(1n << BigInt(magnitude)));
        const tint = draw.u64();
        const channel = (shift) => Math.fround(Math.fround(0.27) + Number((tint >> shift) & 0x6fn) / 512);
        assets.push({
            name: qualifier + '_' + noun + '_' + number + '.' + kind.extension,
            sizeText: sizeLabel(sizeBytes),
            type: kind.type,
            sizeBytes,
            tint: [channel(0n), channel(8n), channel(16n)].join(' '),
            icon: kind.icon,
        });
    }
    return assets;
}

// Returns the key of a group's own row in the collection's tree, or of the row of its asset `child`.
export function collectionTreeKey(group, child = -1) {
    return (group << 16) | (child + 1);
}

// Returns the group of a collection tree's key.
export function collectionGroupOf(key) {
    return key >> 16;
}

// Returns the asset that a collection tree's key names, or -1 if it names its group's own row.
export function collectionAssetOf(key) {
    const child = key & 0xffff;
    return child === 0 ? -1 : collectionGroupOf(key) * groupSize + child - 1;
}

// Returns the keys of the collection tree's visible rows: each group, and the assets of each open group. `groupOpen`
// says whether each group is open.
export function collectionsTreeOrder(groupOpen) {
    const keys = [];
    for (let group = 0; group < groupCount; ++group) {
        keys.push(collectionTreeKey(group));
        if (!groupOpen[group])
            continue;
        for (let child = 0; child < groupSize; ++child)
            keys.push(collectionTreeKey(group, child));
    }
    return keys;
}

// Returns `selection`, a set of values, after a view's edit: `op` is how the `count` rows from `first` join it, as the
// view's `select` names it. `keys` gives the value of each row, or is null if a row's index is its value.
export function selectionApply(selection, op, first, count, keys) {
    const applied = new Set(op === 'replace' ? [] : selection);
    for (let index = first; index < first + count; ++index) {
        const value = keys !== null ? keys[index] : index;
        if (!applied.has(value))
            applied.add(value);
        else if (op === 'toggle')
            applied.delete(value);
    }
    return applied;
}
