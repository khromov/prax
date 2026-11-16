<!-- The gallery's sectionSpace (examples/studio_gallery/section_foundations.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { height, space, text } from '../tokens.js';

    const spaces = [
        { key: '--s-1', value: '2px', px: 2 },
        { key: '--s-2', value: '4px', px: 4 },
        { key: '--s-3', value: '6px', px: 6 },
        { key: '--s-4', value: '8px', px: 8 },
        { key: '--s-5', value: '10px', px: 10 },
        { key: '--s-6', value: '12px', px: 12 },
        { key: '--s-8', value: '16px', px: 16 },
        { key: '--s-10', value: '20px', px: 20 },
        { key: '--s-12', value: '24px', px: 24 },
        { key: '--s-16', value: '32px', px: 32 },
    ];

    const heights = [
        { key: '--h-xs', value: '18px', size: 'xs' },
        { key: '--h-sm', value: '22px', size: 'sm' },
        { key: '--h-md', value: '26px', size: 'md' },
        { key: '--h-lg', value: '30px', size: 'lg' },
    ];

    // The weights of the two splits' panes (gallery_state.h), which a drag of a band changes.
    let splitWeights = $state([1, 1]);
    let splitThirds = $state([1, 2, 1]);

    // Returns the weights that a split's `change` gives, one for each pane.
    function weightsOf(event) {
        return event.text.split(' ').map(Number);
    }
</script>

{#snippet spacingScale()}
  <DemoPanel title="SPACING SCALE" tags="2PX BASE">
    {#each spaces as step (step.key)}
      <box direction="row" align="center" gap={space[3]} height={height.sm} flex="fixed">
        <box direction="row" width={60} flex="fixed"><label {...text.caption}>{step.key}</label></box>
        <box direction="row" width={50} flex="fixed"><label {...text.meta}>{step.value}</label></box>
        <swatch color="accent" size={space[6]} width={step.px} flex="fixed"></swatch>
      </box>
    {/each}
  </DemoPanel>
{/snippet}

{#snippet controlHeights()}
  <DemoPanel title="CONTROL HEIGHTS" tags="4 STEPS">
    {#each heights as step (step.key)}
      <box direction="row" align="center" gap={space[4]} height={space[16]} flex="fixed">
        <box direction="row" width={60} flex="fixed"><label {...text.caption}>{step.key}</label></box>
        <box direction="row" width={50} flex="fixed"><label {...text.meta}>{step.value}</label></box>
        <button size={step.size}>Button</button>
      </box>
    {/each}
  </DemoPanel>
{/snippet}

<!-- A pane of a splitter's specimen, on a background, with a label in it. -->
{#snippet pane(background, label)}
  <split-pane>
    <panel {background} padding={space[2]} flex="fill"><label {...text.caption}>{label}</label></panel>
  </split-pane>
{/snippet}

<SectionFrame number="03" title={'Space & Grid'} lede="A two-pixel base: dense, consistent, predictable.">
  <Grid items={[spacingScale, controlHeights]} columns={2} gap={space[4]}>
    {#snippet cell(panel)}
      {@render panel()}
    {/snippet}
  </Grid>
  <!-- Two shares, and three, of a box that the splits divide by weight. -->
  <DemoPanel title="SPLITTERS" tags="DRAG THE DIVIDERS">
    <split direction="row" height={90} flex="fixed" weights={splitWeights}
        onchange={(event) => (splitWeights = weightsOf(event))}>
      {@render pane('header', 'SHARE A')}
      {@render pane('panel', 'SHARE B')}
    </split>
    <split direction="row" height={90} flex="fixed" weights={splitThirds}
        onchange={(event) => (splitThirds = weightsOf(event))}>
      {@render pane('header', '1')}
      {@render pane('panel', '2')}
      {@render pane('header', '1')}
    </split>
  </DemoPanel>
</SectionFrame>
