<!-- The gallery's sectionTooltips (examples/studio_gallery/section_feedback.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';
    import SpecimenRow from './feedback/SpecimenRow.svelte';
    import { bodySmall } from './feedback/styles.js';

    // The buttons that show a tooltip.
    const tipButtons = [
        { label: 'Move Tool', shortcut: 'W', icon: 'move' },
        { label: 'Rotate Tool', shortcut: 'E', icon: 'rotate' },
        { label: 'Scale Tool', shortcut: 'R', icon: 'scale' },
        { label: 'Toggle Visibility', shortcut: '', icon: 'eye' },
        { label: 'Lock Selection', shortcut: '', icon: 'lock' },
    ];

    const description =
        "A tooltip shows once the pointer has rested on its anchor, places itself from that anchor's last box " +
        'and stays inside the frame. The anatomy beside it is the same chip, drawn in the flow.';
</script>

<SectionFrame number="16" title="Tooltips" lede="A name and its chord, after a short rest." {description}>
  <DemoPanel title="ANATOMY" tags="IN THE FLOW">
    <SpecimenRow gap={space[3]}>
      {#each tipButtons as tip (tip.label)}
        <panel background="hud" rules="all" direction="row" align="center" gap={space[3]}
            padding="{space[1]} {space[3]}">
          <label {...bodySmall}>{tip.label}</label>
          {#if tip.shortcut}
            <kbd>{tip.shortcut}</kbd>
          {/if}
        </panel>
      {/each}
    </SpecimenRow>
  </DemoPanel>
  <DemoPanel title="HOVER" tags="DELAY . SHORTCUT">
    <SpecimenRow gap={space[3]}>
      {#each tipButtons as tip (tip.label)}
        <button icon={tip.icon}><tooltip shortcut={tip.shortcut}>{tip.label}</tooltip></button>
      {/each}
    </SpecimenRow>
    <label {...text.caption}>REST ON A BUTTON ABOVE</label>
  </DemoPanel>
</SectionFrame>
