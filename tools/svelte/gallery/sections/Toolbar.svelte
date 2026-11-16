<!-- The gallery's sectionToolbar (examples/studio_gallery/section_components.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { height, space, text } from '../tokens.js';

    const tools = [
        { tip: 'Select', icon: 'cursor' },
        { tip: 'Move', icon: 'move' },
        { tip: 'Rotate', icon: 'rotate' },
        { tip: 'Scale', icon: 'scale' },
    ];

    const spaces = ['Local', 'World'];

    // Each cell is shown selected, so that its background shows its size.
    const steps = [
        { size: 'xs', name: 'XS 18' },
        { size: 'sm', name: 'SM 22' },
        { size: 'md', name: 'MD 26' },
        { size: 'lg', name: 'LG 30' },
    ];

    // The viewport toolbar's tool, space and transport.
    let toolbar = $state({ tool: 1, space: 1, snap: false, grid: true, playing: false });
</script>

<SectionFrame number="12" title="Toolbar" lede="The tool row and the transport beside it.">
  <DemoPanel title="VIEWPORT TOOLBAR" tags="MAIN" bare>
    <panel background="panel" direction="row" align="center" gap={space[1]} padding="0 {space[3]}" height={height.lg}
        flex="fixed">
      {#each tools as tool, i (tool.tip)}
        <button icon={tool.icon} selected={i === toolbar.tool} variant="ghost" size="md"
            onclick={() => (toolbar.tool = i)}>
          <tooltip>{tool.tip}</tooltip>
        </button>
      {/each}
      <divider></divider>
      <segmented size="sm">
        {#each spaces as label, i (label)}
          <segment selected={toolbar.space === i} onclick={() => (toolbar.space = i)}>{label}</segment>
        {/each}
      </segmented>
      <divider></divider>
      <button icon="magnet" selected={toolbar.snap} variant="ghost" size="md"
          onclick={() => (toolbar.snap = !toolbar.snap)}></button>
      <button icon="grid" selected={toolbar.grid} variant="ghost" size="md"
          onclick={() => (toolbar.grid = !toolbar.grid)}></button>
      <label {...text.caption}>0.25m</label>
      <spacer></spacer>
      <button icon={toolbar.playing ? 'pause' : 'play'} selected={toolbar.playing} variant="ghost" size="md"
          onclick={() => (toolbar.playing = !toolbar.playing)}></button>
      <button icon="step" variant="ghost" size="md"></button>
      <button icon="stop" variant="ghost" size="md" onclick={() => (toolbar.playing = false)}></button>
    </panel>
  </DemoPanel>
  <DemoPanel title="CELL SIZES" tags="XS . SM . MD . LG">
    <box direction="row" align="end" gap={space[8]}>
      {#each steps as step (step.size)}
        <box align="center" gap={space[2]}>
          <button icon="magnet" selected variant="ghost" size={step.size}></button>
          <label {...text.caption}>{step.name}</label>
        </box>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
