<!-- The gallery's sectionTags (examples/studio_gallery/section_components.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    // The labels that the chip list adds, in turn.
    const chipPool = ['lod0', 'static', 'baked', 'nav', 'trigger', 'decal'];

    const chords = ['Cmd', 'Shift', 'K', 'Ret', 'Esc', 'F2'];

    // The removable chips, which the gallery's model seeds.
    let chips = $state(['player', 'physics', 'no-lod', 'shader:pbr']);

    // Answers the button beside the chips as the gallery does. The gallery adds the pool's next label, and then removes
    // the chip at the index that it holds for the chip to remove. That index starts as the number of chips, which
    // stands for no chip, and the add does not update it. So the gallery removes the chip that it added, and its button
    // leaves the chips as they were. The port keeps that, since the gallery is its reference.
    function added() {
        const removeAt = chips.length;
        chips.push(chipPool[chips.length % chipPool.length]);
        if (removeAt < chips.length)
            chips.splice(removeAt, 1);
    }
</script>

<SectionFrame number="14" title="Tags & Badges" lede="Small metadata, clearly typed.">
  <DemoPanel title="BADGES" tags="STATIC LABEL">
    <box direction="row" align="center" wrap gap={space[3]}>
      <badge variant="neutral">DEFAULT</badge>
      <badge variant="accent">ACCENT</badge>
      <badge variant="success">BUILT</badge>
      <badge variant="warning">PENDING</badge>
      <badge variant="danger">ERROR</badge>
      <badge variant="neutral">v0.1.0</badge>
      <badge variant="neutral">0x7FF8_A2B0</badge>
    </box>
    <box direction="row" align="center" gap={space[3]}>
      <status tone="warning">M</status>
      <status tone="success">A</status>
      <status tone="danger">D</status>
      <status tone="accent">R</status>
      <label {...text.bodySecondary}>the one or two character status a row wears</label>
    </box>
  </DemoPanel>
  <DemoPanel title="KBD" tags="KEY CHIPS">
    <box direction="row" align="center" gap={space[3]}>
      {#each chords as chord (chord)}
        <kbd>{chord}</kbd>
      {/each}
      <label {...text.bodySecondary}>the key caps that menus and tooltips show</label>
    </box>
  </DemoPanel>
  <DemoPanel title="CHIPS" tags="REMOVABLE . READ-ONLY">
    <box direction="row" align="center" wrap gap={space[3]}>
      <!-- A chip is the chip of its place, as the gallery's is, so the chips after a removed one take its place. -->
      {#each chips as chip, i}
        <chip removable onremove={() => chips.splice(i, 1)}>{chip}</chip>
      {/each}
      <button icon="plus" variant="ghost" size="xs" onclick={added}>Tag</button>
    </box>
    <box direction="row" align="center" wrap gap={space[3]}>
      {#each chipPool as label (label)}
        <chip>{label}</chip>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
