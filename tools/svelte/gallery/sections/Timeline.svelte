<!-- The gallery's sectionTimeline (examples/studio_gallery/section_data.cpp).

     The example does two things below the kit that the binding does not offer, which are done here as nearly as a
     script can do them.

     Playback. The example advances the playhead by the delta of each frame's first round. A script cannot read that
     delta: it has timers, and the page's clock, which tells time in steps of a tenth of a millisecond. The playhead
     here advances on a timer that runs once before each frame, by the time that passed since it last ran. A time that
     is a sixtieth of a second to within the clock's step counts as a sixtieth, which is the delta of the host's frames.

     Undo. The example records its Undo from the edit sink of the panel (prUiSetEditSink), which hears each gesture
     begin, change and commit. A script hears what a gesture changed, in the events of the timeline and of its blocks,
     and not where the gesture begins or ends. A drag's gesture here begins with its first change and commits once no
     change has come for `gesturePause`. The script therefore misses a gesture that changes nothing, begins a gesture
     later than the example if the drag's first moves change nothing, and commits it later than the release. It also
     cannot tell a marquee's selection, which is a gesture's change, from a press's, which is not, so no selection is a
     change here. -->
<script>
    import { onDestroy } from 'svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { seedTimeline } from '../state/timeline.js';
    import { height, space, text } from '../tokens.js';

    const description =
        "The kit's timeline draws the ruler, the lanes, the blocks, the markers, the loop band and the playhead, " +
        'and writes the model that this example owns. Drag the ruler to scrub the playhead, a block to move it, ' +
        "a block's end to resize it, and an empty part of a track to select with a rectangle; double-click a " +
        'track to add a block, and Delete removes what is selected. A drag snaps to the grid, and to the edges ' +
        'of other blocks, the markers and the playhead as the toolbar enables them, and a line marks a snap to ' +
        'any of those. The wheel pans the view and Cmd with the wheel zooms it. Every gesture reports to the ' +
        'edit sink, which is what Undo steps back.';

    // The seconds between the host's frames, and the step of the page's clock, in milliseconds.
    const frameDelta = 1 / 60;
    const clockStep = 0.1;

    // The longest time between two changes of one drag's gesture, in milliseconds.
    const gesturePause = 250;

    // The timeline section's clip: its tracks, blocks and markers, the view, the playhead and the loop, the snapping
    // that the toolbar turns on, and the undo state. A gesture copies the blocks that it opens on, and one that changed
    // them becomes the state that Undo restores. A time that the example keeps in single precision is that number here.
    const seed = seedTimeline();
    const tracks = seed.tracks;
    const markers = seed.markers;
    const loop = [2, 6];
    let blocks = $state(seed.blocks);
    let opened = [];
    let undo = [];
    let view = $state.raw([0, 8]);
    let inspectorView = $state.raw([0, 8]); // the read-only timeline's own view
    let playhead = $state(1.5);
    let nextId = seed.nextId;
    let begins = $state(0);
    let changes = $state(0);
    let commits = $state(0);
    let playing = $state(false);
    let snapping = $state(true);
    let snapEdges = $state(true);
    let snapMarkers = $state(true);
    let snapPlayhead = $state(true);
    let hasUndo = $state(false);
    let wrote = false; // the open gesture changed the blocks

    const snaps = [
        { label: 'Grid 0.25s', value: () => snapping, set: (on) => (snapping = on) },
        { label: 'Edges', value: () => snapEdges, set: (on) => (snapEdges = on) },
        { label: 'Markers', value: () => snapMarkers, set: (on) => (snapMarkers = on) },
        { label: 'Playhead', value: () => snapPlayhead, set: (on) => (snapPlayhead = on) },
    ];

    const selected = $derived(blocks.filter((block) => block.selected).length);
    const tally = $derived([
        { label: 'BEGIN', value: begins },
        { label: 'CHANGE', value: changes },
        { label: 'COMMIT', value: commits },
    ]);

    // The playhead runs through the loop while the clip plays.
    $effect(() => {
        if (!playing)
            return undefined;
        let last = performance.now();
        let timer = setTimeout(function advance() {
            const now = performance.now();
            const elapsed = now - last;
            const delta = Math.abs(elapsed - frameDelta * 1000) <= clockStep ? frameDelta : elapsed / 1000;
            playhead = Math.fround(playhead + Math.fround(delta));
            if (playhead >= loop[1])
                playhead = loop[0];
            last = now;
            timer = setTimeout(advance, 1);
        }, 1);
        return () => clearTimeout(timer);
    });

    // Records a phase of a gesture for the timeline's Undo, as the example's edit sink does.
    function timelineEdit(phase) {
        if (phase === 'begin') {
            opened = $state.snapshot(blocks);
            wrote = false;
            ++begins;
        } else if (phase === 'change') {
            wrote = true;
            ++changes;
        } else {
            if (wrote) {
                undo = opened;
                hasUndo = true;
            }
            ++commits;
        }
    }

    // Whether a drag's gesture is open, and the timer that commits it once its changes pause.
    let live = false;
    let pause = 0;
    onDestroy(() => clearTimeout(pause));

    function gestureCommit() {
        if (!live)
            return;
        clearTimeout(pause);
        live = false;
        timelineEdit('commit');
    }

    // Records a gesture that makes one write, as the kit reports a request for a block or for a removal.
    function gestureOnce() {
        gestureCommit();
        timelineEdit('begin');
        timelineEdit('change');
        timelineEdit('commit');
    }

    // The moves and resizes that the open round's events tell, each a block with its new start and duration, or null
    // if the round has told no change of a drag.
    let moves = null;

    // Returns the open round's moves, after noting that the round holds a change of a drag: a scrub, or the moves and
    // resizes of blocks. A round is one change of its gesture, however many blocks it moved. All of a round's events
    // are heard before the script yields, so the blocks are moved once they are. A gesture that the round begins then
    // copies the blocks as the round selected them and before it moved them, which is what the example's sink copies.
    function dragged() {
        if (moves !== null)
            return moves;
        moves = [];
        queueMicrotask(() => {
            if (!live)
                timelineEdit('begin');
            live = true;
            for (const { block, start, duration } of moves)
                Object.assign(block, { start, duration });
            moves = null;
            timelineEdit('change');
            clearTimeout(pause);
            pause = setTimeout(gestureCommit, gesturePause);
        });
        return moves;
    }

    function scrub(event) {
        playhead = event.value;
        dragged();
    }

    function move(block, event) {
        const [start, duration] = event.values;
        dragged().push({ block, start, duration });
    }

    function add(event) {
        const [track, start] = event.values;
        gestureOnce();
        blocks.push({ id: nextId++, track, start, duration: 1, color: '0.47 0.7 0.47', label: 'New', selected: false });
    }

    function remove() {
        gestureOnce();
        blocks = blocks.filter((block) => !block.selected);
    }

    function undoStep() {
        gestureCommit();
        blocks = undo.map((block) => ({ ...block }));
        hasUndo = false;
    }
</script>

<!-- The clip's tracks, with the blocks and the markers of each. `edited` is true for the timeline that edits them. -->
{#snippet lanes(edited)}
  {#each tracks as name, track (name)}
    <timeline-track>
      {name}
      {#each blocks.filter((block) => block.track === track) as block (block.id)}
        <timeline-block start={block.start} duration={block.duration} color={block.color} selected={block.selected}
            onchange={edited ? (event) => move(block, event) : null}
            onselect={edited ? (event) => (block.selected = event.value === 1) : null}>
          {block.label}
        </timeline-block>
      {/each}
      {#each markers.filter((marker) => marker.track === track) as marker (marker.time)}
        <timeline-marker time={marker.time} color={marker.color}></timeline-marker>
      {/each}
    </timeline-track>
  {/each}
{/snippet}

<SectionFrame number="20" title="Timeline" lede="An editable clip over caller-owned tracks." {description}>
  <panel rules="bottom" direction="row" align="center" gap={space[5]} padding="0 {space[4]}" height={height.md}
      flex="fixed">
    <toggle value={playing} onchange={(event) => (playing = event.value === 1)}>Play</toggle>
    <button size="sm" onclick={() => (playhead = loop[0])}>Reset cursor</button>
    <button size="sm" onclick={() => (view = [0, 8])}>Fit view</button>
    {#each snaps as snap (snap.label)}
      <checkbox value={snap.value()} onchange={(event) => snap.set(event.value === 1)}>{snap.label}</checkbox>
    {/each}
    <spacer></spacer>
    <button size="sm" icon="undo" disabled={!hasUndo} onclick={undoStep}>Undo</button>
    <label {...text.monoMuted}>{selected} selected</label>
  </panel>

  <DemoPanel title="ANIMATION CLIP" tags="EDITABLE . UNDO SINK" bare>
    <timeline {view} {playhead} {loop} snap-step={snapping ? 0.25 : 0} snap-edges={snapEdges}
        snap-markers={snapMarkers} snap-playhead={snapPlayhead} track-height={30} onchange={scrub}
        onview={(event) => (view = event.values)} onadd={add} onremove={remove}>
      {@render lanes(true)}
    </timeline>
    <panel rules="top" direction="row" gap={space[4]} padding={space[4]}>
      {#each tally as count (count.label)}
        <stat-tile value={String(count.value)}>{count.label}</stat-tile>
      {/each}
    </panel>
  </DemoPanel>

  <!-- The same model again, shown and not edited, with its own view. -->
  <DemoPanel title="STATE INSPECTOR" tags="READ-ONLY" bare>
    <timeline view={inspectorView} {playhead} {loop} snap-step={snapping ? 0.25 : 0} snap-edges={snapEdges}
        snap-markers={snapMarkers} snap-playhead={snapPlayhead} read-only track-height={22}
        onview={(event) => (inspectorView = event.values)}>
      {@render lanes(false)}
    </timeline>
  </DemoPanel>
</SectionFrame>
