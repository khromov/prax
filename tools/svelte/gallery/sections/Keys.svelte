<!-- The gallery's sectionKeys (examples/studio_gallery/section_feedback.cpp).

     The example gives its third row the first row's key (prUiKey) while the toggle is on, and lib/ui outlines the two
     rows. A node of the binding has a key of its own, which no attribute names, so the rows here never share one and
     the toggle changes nothing but itself. -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space } from '../tokens.js';

    // Whether the section gives two of its rows one key (gallery_state.h KeysState).
    let duplicate = $state(false);

    const rows = [
        { name: 'row-a', label: 'Transform' },
        { name: 'row-b', label: 'Mesh Renderer' },
        { name: 'row-c', label: 'Rigidbody' },
    ];

    const description =
        "Two widgets of one round that share a key are a caller's error. Layout makes the second node without the " +
        'key, so the tree stays whole, and lib/ui outlines both nodes in the next round.';
</script>

<SectionFrame number="23" title="Duplicate Keys" lede="A key names one node of a round." {description}>
  <DemoPanel title="ROWS" tags="LIVE">
    <toggle value={duplicate} onchange={(event) => (duplicate = event.value === 1)}>
      Give the third row the first row's key
    </toggle>
    <box gap={space[2]}>
      {#each rows as row (row.name)}
        <panel background="raised" padding="{space[3]} {space[4]}"><label>{row.label}</label></panel>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
