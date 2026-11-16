// What the gallery's menu specimens chose and the command palette's query (gallery_state MenuState and PaletteState),
// with the lists that the selects and the palette show.

// What the menu specimens chose. `tag` is a swatch's color: the color that the model starts with, then the name of the
// palette's color that the context menu's row of tags chose.
export const menu = $state({
    verb: '',
    component: 0,
    clip: 34,
    outsideClicks: 0,
    tag: '0.35 0.55 0.82',
    wrap: false,
});

// The command palette's query and selected command.
export const palette = $state({
    query: '',
    active: 0,
});

export const componentTypes = [
    'Transform', 'Mesh Renderer', 'Collider',
    'Rigidbody', 'Script', 'Audio Source',
    'Camera',
];

export const animationClips = [
    'Idle', 'Idle Breathe', 'Idle Look Left', 'Idle Look Right', 'Idle Scratch',
    'Idle Stretch', 'Idle Yawn', 'Idle Impatient', 'Idle Crouch', 'Idle Alert',
    'Walk Forward', 'Walk Back', 'Walk Left', 'Walk Right', 'Walk Slow',
    'Walk Injured', 'Walk Aim', 'Walk Crouch', 'Walk Turn Left', 'Walk Turn Right',
    'Run Forward', 'Run Back', 'Run Left', 'Run Right', 'Run Aim',
    'Run Turn Left', 'Run Turn Right', 'Sprint', 'Sprint Stop', 'Sprint Slide',
    'Jump Start', 'Jump Loop', 'Jump Land', 'Jump Land Hard', 'Jump Land Roll',
    'Fall Loop', 'Fall Recover', 'Vault Low', 'Vault High', 'Climb Ladder Up',
    'Climb Ladder Down', 'Climb Ledge', 'Swim Idle', 'Swim Forward', 'Swim Dive',
    'Attack Light 1', 'Attack Light 2', 'Attack Light 3', 'Attack Heavy', 'Attack Spin',
    'Block Start', 'Block Loop', 'Block Impact', 'Parry', 'Dodge Roll',
    'Dodge Step Back', 'Hit React Front', 'Hit React Back', 'Hit React Left', 'Hit React Right',
    'Death Front', 'Death Back', 'Emote Wave', 'Emote Point', 'Emote Salute',
    'Emote Sit',
];

// The palette's commands. A command with no chord has no `shortcut`.
export const commands = [
    { label: 'New Scene', category: 'File', shortcut: 'Cmd N', icon: 'folder' },
    { label: 'Open Scene...', category: 'File', shortcut: 'Cmd O', icon: 'folder' },
    { label: 'Save', category: 'File', shortcut: 'Cmd S', icon: 'folder' },
    { label: 'Undo', category: 'Edit', shortcut: 'Cmd Z', icon: 'undo' },
    { label: 'Duplicate Entity', category: 'Edit', shortcut: 'Cmd D', icon: 'script' },
    { label: 'Toggle Gizmos', category: 'View', shortcut: 'Cmd ;', icon: 'eye' },
    { label: 'Focus Selected', category: 'View', shortcut: 'F', icon: 'target' },
    { label: 'Create Empty', category: 'Entity', shortcut: 'Shift Cmd N', icon: 'cube' },
    { label: 'Add Mesh Renderer', category: 'Entity', icon: 'cube' },
    { label: 'Build and Run', category: 'Build', shortcut: 'Cmd B', icon: 'play' },
];

// Returns `text` with its ASCII letters in lower case, which is how the gallery's model compares a query.
function fold(text) {
    return text.replace(/[A-Z]/g, (letter) => letter.toLowerCase());
}

function containsIgnoreCase(text, needle) {
    return needle === '' || fold(text).includes(fold(needle));
}

// Returns true if the command's label or category holds `query`, ignoring case. An empty query matches every command.
export function commandMatches(item, query) {
    return containsIgnoreCase(item.label, query) || containsIgnoreCase(item.category, query);
}
