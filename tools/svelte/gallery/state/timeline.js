// The timeline section's clip (examples/studio_gallery/gallery_state.cpp): its tracks, its blocks and its markers.

// A block of the timeline's clip. A color is its channels, as a color attribute takes them.
const blockSeeds = [
    { label: 'Idle', track: 0, start: 0, duration: 3, color: '0.38 0.58 0.82' },
    { label: 'Walk', track: 0, start: 3.25, duration: 2.5, color: '0.38 0.58 0.82' },
    { label: 'Twist', track: 1, start: 1, duration: 2, color: '0.59 0.46 0.78' },
    { label: 'Wave', track: 2, start: 2.5, duration: 3, color: '0.82 0.55 0.35' },
];

const trackSeeds = ['Root', 'Spine', 'Arm.L', 'Events'];

// The times of the events on the last track, in seconds.
const markerSeeds = [1.5, 4, 5.5];

// Returns the clip: the names of its tracks, its blocks, its markers, and the id of the next block that the user adds.
export function seedTimeline() {
    let nextId = 1;
    return {
        tracks: trackSeeds,
        blocks: blockSeeds.map((seed) => ({ id: nextId++, ...seed, selected: false })),
        markers: markerSeeds.map((time) => ({ track: 3, time, color: '0.86 0.38 0.38' })),
        nextId,
    };
}
