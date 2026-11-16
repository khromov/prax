// The values that the inputs section's specimens are bound to, and the helpers that edit them: the gallery's
// InputsState, UndoState and UndoSnapshot (examples/studio_gallery/gallery_state.h), with the values that
// gallery_state.cpp gives them.

// The area paths and the revisions that the suggest fields offer (galleryAreas, galleryRevisions).
export const areas = [
    'lib/app',
    'lib/core',
    'lib/draw',
    'lib/gpu',
    'lib/image',
    'lib/layout',
    'lib/ui',
    'lib/studio/kit',
    'lib/html',
    'tools/suite',
];

export const revisions = ['main', 'origin/main', 'shadow-atlas', 'v0.4.1', '9f3c1ad', '2b70e44', 'c81da09'];

// The text, numeric, vector and color values of the inputs section. A value that the example holds as a float is the
// float nearest to the number here, which is what a control of the type f32 stores it as.
export const inputs = $state({
    name: 'MainCamera',
    search: 'level_01',
    locked: 'Locked',
    command: '',
    area: 'lib/gpu',
    revision: 'main',
    prose:
        'Soft wrap breaks this paragraph at the field width, so the caret, the selection bands and Up and ' +
        'Down all follow the visual lines rather than the single line the text is. Double-click a word, ' +
        'triple-click the line, and hold the drag to grow the selection by that unit.',
    snippet: 'vec3 n = normalize(worldNormal);\nfloat d = dot(n, lightDir);\nreturn baseColor * max(d, 0.0);',

    // The text of the styled area, and the style that all of it has, which indexes the area's styles.
    styled: {
        text:
            'Select some text and give it a style. Each style is a run of its own, and the ' +
            'caret, the selection and undo count through them all.',
        style: 0,
    },

    // The states of the styled area that its Undo and Redo step through, oldest first, each a copy of `styled`, and
    // the one that `styled` is.
    styledHistory: [],
    styledCursor: 0,

    // Submitted lines, oldest first.
    commands: ['build --release', 'suite render css/flex-basis', 'bench vg'],

    uv: [0.5, 0.5],
    position: [1.234, 0.0, -4.5],
    rotation: [0.0, 90.0, 0.0],
    quaternion: [0.0, 0.0, 0.0, 1.0],
    scalar: 0.75,
    mass: 80.0,
    shutter: 0.016,
    frequency: 440.0,
    shutterSeconds: Math.fround(1.0 / 60.0),
    tiny: -12,
    level: 200,
    offset: -1200,
    stride: 40000,
    subdivisions: 3,
    budget: -5000000,
    seed: 1024,
    cycles: 9000000,
    density: 80.0,

    // The undo history that the edit lifecycle panel records. `values` are what the panel binds, and one state is a
    // copy of them. A gesture copies the values that it opens on and commits one state if it changed them. An edit
    // after an undo drops the states after it.
    undo: {
        values: { strength: 0.4, steps: 8, enabled: true },
        opened: { strength: 0.4, steps: 8, enabled: true },
        history: [], // oldest first
        cursor: 0, // the state that `values` is
        begins: 0,
        changes: 0,
        commits: 0,
        wrote: false, // the open gesture changed `values`
    },

    tint: [0.42, 0.55, 0.78, 1.0],
});

// Appends the command line in flight to the history and empties it. An empty line adds nothing.
export function submitCommand() {
    if (inputs.command === '')
        return;
    inputs.commands.push(inputs.command);
    inputs.command = '';
}

// Begins an edit gesture, copying the values it opens on.
export function undoBegin(undo) {
    undo.opened = { ...undo.values };
    undo.wrote = false;
    if (undo.history.length === 0) {
        undo.history.push({ ...undo.opened });
        undo.cursor = 0;
        return;
    }
    undo.history[undo.cursor] = { ...undo.opened };
}

// Commits a gesture that changed the values, dropping the states after an undo.
export function undoCommit(undo) {
    if (!undo.wrote)
        return;
    undo.history.length = undo.cursor + 1;
    undo.history.push({ ...undo.values });
    undo.cursor = undo.history.length - 1;
}

// Return true if there is a state to undo to, or to redo to.
export function undoAvailable(undo) {
    return undo.cursor > 0;
}

export function redoAvailable(undo) {
    return undo.cursor + 1 < undo.history.length;
}

// Steps the history one state back, or forward if `forward` is true, and writes that state to the values. Returns
// false if there is no state to step to.
export function undoStep(undo, forward) {
    if (!(forward ? redoAvailable(undo) : undoAvailable(undo)))
        return false;
    undo.cursor = forward ? undo.cursor + 1 : undo.cursor - 1;
    undo.values = { ...undo.history[undo.cursor] };
    return true;
}

// Records the styled area's state as the step after the one it is at, dropping the states after an undo. The first
// record keeps the state that the area started in before it.
export function styledRecord(before) {
    if (inputs.styledHistory.length === 0)
        inputs.styledHistory.push({ ...before });
    inputs.styledHistory.length = inputs.styledCursor + 1;
    inputs.styledHistory.push({ ...inputs.styled });
    inputs.styledCursor = inputs.styledHistory.length - 1;
}

// Steps the styled area one state back, or forward if `forward` is true, if there is a state to step to.
export function styledStep(forward) {
    const cursor = inputs.styledCursor + (forward ? 1 : -1);
    if (cursor < 0 || cursor >= inputs.styledHistory.length)
        return;
    inputs.styledCursor = cursor;
    inputs.styled = { ...inputs.styledHistory[cursor] };
}
