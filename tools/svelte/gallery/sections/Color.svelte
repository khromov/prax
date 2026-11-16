<!-- The gallery's sectionColor (examples/studio_gallery/section_foundations.cpp). -->
<script>
    import Card from '../parts/Card.svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import FillLabel from '../parts/FillLabel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    // The stops of the accent ramp: each is the accent mixed toward white (a positive amount) or black (a negative
    // one).
    const ramp = [
        { name: 'accent-50', amount: 0.9 },
        { name: 'accent-100', amount: 0.75 },
        { name: 'accent-200', amount: 0.6 },
        { name: 'accent-300', amount: 0.4 },
        { name: 'accent-400', amount: 0.2 },
        { name: 'accent-500', amount: 0, note: 'primary' },
        { name: 'accent-600', amount: -0.2 },
        { name: 'accent-700', amount: -0.4 },
        { name: 'accent-800', amount: -0.6 },
        { name: 'accent-900', amount: -0.75 },
    ];

    // The gamuts of the three pickers, each by the keyword that a picker takes.
    const gamuts = [
        { name: 'sRGB', space: 'srgb' },
        { name: 'Display P3', space: 'display-p3' },
        { name: 'Rec. 2020', space: 'rec2020' },
    ];

    // The steps of the panel ramp.
    const panels = [
        { label: 'App', key: 'bg-0', background: 'app' },
        { label: 'Panel', key: 'bg-1', background: 'panel' },
        { label: 'Header', key: 'bg-2', background: 'header' },
        { label: 'Raised', key: 'bg-3', background: 'raised' },
        { label: 'Select', key: 'accent-weak', background: 'select' },
        { label: 'Viewport', key: 'bg-viewport', background: 'viewport' },
    ];

    const levels = [
        { name: 'Primary - The quick brown fox', key: 'text-1', tone: 'primary' },
        { name: 'Secondary - The quick brown fox', key: 'text-2', tone: 'secondary' },
        { name: 'Tertiary - The quick brown fox', key: 'text-3', tone: 'muted' },
        { name: 'Disabled - The quick brown fox', key: 'text-4', tone: 'faint' },
    ];

    // The semantic colors, each a color of the palette by the name that a swatch takes.
    const semantics = [
        { name: 'Success', key: 'success', color: 'success' },
        { name: 'Warning', key: 'warning', color: 'warning' },
        { name: 'Error', key: 'danger', color: 'danger' },
        { name: 'Accent', key: 'accent', color: 'accent' },
    ];

    // The height of a band of color, in px.
    const bandHeight = 56;

    // The color that the picker and the field edit together (gallery_state.h), as the channels of extended sRGB.
    let color = $state([0.42, 0.55, 0.78, 1]);

    // The colors of the pickers in sRGB, Display P3 and Rec. 2020, each starting at P3's green primary in extended
    // sRGB, which lies past sRGB.
    let gamutColors = $state([
        [-0.511, 1.018, -0.311, 1],
        [-0.511, 1.018, -0.311, 1],
        [-0.511, 1.018, -0.311, 1],
    ]);

    // Returns a swatch's color for the palette's color `name` mixed toward white (a positive amount) or black (a
    // negative one). The example mixes the theme's color itself. A script cannot read the theme, so it names the
    // color and its shade, and the kit mixes them in the theme that it shows.
    function shade(name, amount) {
        return name + ' ' + amount;
    }

    const description =
        'The neutral ramp reads as a tool rather than a toy. The accent carries selection, focus and the primary ' +
        'action, and nothing else; the semantic colors are reserved for state.';

    const gamutsNote =
        'The same green in a picker of each gamut. A plane shows a color past its gamut mapped into ' +
        "it, a line marks the gamut's edge, and a dashed line where sRGB ends inside a wider one. A " +
        'display shows the colors of a gamut wider than its own mapped into its own.';
</script>

<!-- A band of a color over its name and caption. -->
{#snippet rampCell(name, band, note)}
  <Card>
    <swatch color={band} size={bandHeight} width="fill" flex="fixed"></swatch>
    <box gap={space[1]} padding={space[4]}>
      <label {...text.caption}>{name}</label>
      {#if note}
        <label {...text.metaMuted}>{note}</label>
      {/if}
    </box>
  </Card>
{/snippet}

{#snippet textLevels()}
  <DemoPanel title="TEXT" tags="4 LEVELS">
    {#each levels as level (level.key)}
      <panel rules="bottom" direction="row" align="center" gap={space[2]} padding="{space[2]} 0" flex="fixed">
        <FillLabel tone={level.tone}>{level.name}</FillLabel>
        <label {...text.metaMuted}>{level.key}</label>
      </panel>
    {/each}
  </DemoPanel>
{/snippet}

{#snippet semanticColors()}
  <DemoPanel title="SEMANTIC" tags="STATE">
    {#each semantics as semantic (semantic.key)}
      <panel rules="bottom" direction="row" align="center" gap={space[3]} padding="{space[2]} 0" flex="fixed">
        <swatch color={semantic.color} size={space[6]}></swatch>
        <FillLabel>{semantic.name}</FillLabel>
        <label {...text.metaMuted}>{semantic.key}</label>
      </panel>
    {/each}
  </DemoPanel>
{/snippet}

<SectionFrame number="01" title="Color" lede="Low-saturation neutrals. One signature accent." {description}>
  <DemoPanel title="ACCENT RAMP" tags="10 STOPS">
    <Grid items={ramp} columns={5} gap={space[2]}>
      {#snippet cell(stop)}
        {@render rampCell(stop.name, shade('accent', stop.amount), stop.note)}
      {/snippet}
    </Grid>
  </DemoPanel>
  <DemoPanel title="COLOR PICKER" tags="HSV . OKLCH . RAMP">
    <box direction="row" align="start" gap={space[8]}>
      <color-picker value={color} gamut="display" onchange={(event) => (color = event.values)}></color-picker>
      <box gap={space[3]}>
        <label {...text.bodySecondary}>The same color in a field, which opens the picker in a popover:</label>
        <color-field value={color} width={220} flex="fixed" onchange={(event) => (color = event.values)}></color-field>
      </box>
    </box>
  </DemoPanel>
  <DemoPanel title="GAMUTS" tags="SRGB . DISPLAY P3 . REC. 2020">
    <label {...text.bodySecondary}>{gamutsNote}</label>
    <box direction="row" align="start" gap={space[8]}>
      {#each gamuts as gamut, index (gamut.space)}
        <box gap={space[2]}>
          <label {...text.strong}>{gamut.name}</label>
          <color-picker value={gamutColors[index]} mode="oklch" gamut={gamut.space} opaque
              onchange={(event) => (gamutColors[index] = event.values)}></color-picker>
        </box>
      {/each}
    </box>
  </DemoPanel>
  <DemoPanel title="PANEL RAMP" tags="6 STEPS" bare>
    <box direction="row">
      {#each panels as step (step.key)}
        <panel background={step.background} rules="right" gap={space[1]} padding="{space[3]} {space[4]}" flex="fill">
          <label {...text.strong}>{step.label}</label>
          <label {...text.caption}>{step.key}</label>
        </panel>
      {/each}
    </box>
  </DemoPanel>
  <Grid items={[textLevels, semanticColors]} columns={2} gap={space[4]}>
    {#snippet cell(panel)}
      {@render panel()}
    {/snippet}
  </Grid>
</SectionFrame>
