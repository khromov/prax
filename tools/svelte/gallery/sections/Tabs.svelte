<!-- The gallery's sectionTabs (examples/studio_gallery/section_controls.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import Segmented from '../parts/Segmented.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { tabs } from '../state/tabs.svelte.js';
    import { space, text } from '../tokens.js';

    // What each tab shows: a label, a badge if it has one, and a close control if it is closable. A tab's id is its
    // place here plus one.
    const panels = [
        { label: 'Scene' },
        { label: 'Game' },
        { label: 'Profiler', badge: '3' },
        { label: 'Animator' },
        { label: 'Shader Graph', closable: true },
    ];

    const bodies = [
        'The scene the viewport renders.',
        'The game view, at the target resolution.',
        'Three frames over budget.',
        'The state machine of the selected rig.',
        'The material graph of the selected mesh.',
    ];

    const modes = [{ label: 'Wireframe' }, { label: 'Solid' }, { label: 'Textured' }, { label: 'PBR' }];
    const shapes = [{ icon: 'cube' }, { icon: 'sphere' }, { icon: 'light' }];
    const spaces = [{ label: 'Local' }, { label: 'World' }];
    const sizes = ['xs', 'sm', 'md'];
</script>

<SectionFrame number="08" title="Tabs & Segmented" lede="Navigation, and mutually exclusive view modes.">
  <DemoPanel title="TABS" tags="PANEL NAV . DRAG TO REORDER" bare>
    <tab-strip reorderable onselect={(event) => (tabs.panel = event.value)}>
      {#each panels as panel, index}
        <tab id={index + 1}>
          <tab-label>{panel.label}</tab-label>
          {#if panel.badge}
            <badge variant="warning">{panel.badge}</badge>
          {/if}
          {#if panel.closable}
            <tab-close></tab-close>
          {/if}
        </tab>
      {/each}
    </tab-strip>
    <box padding={space[5]}><label {...text.bodySecondary}>{bodies[tabs.panel - 1]}</label></box>
  </DemoPanel>
  <DemoPanel title="SEGMENTED" tags="VIEW MODE">
    <box direction="row" align="center" gap={space[3]}>
      <Segmented items={modes} bind:selected={tabs.viewMode} />
      <Segmented items={shapes} bind:selected={tabs.shape} />
    </box>
  </DemoPanel>
  <DemoPanel title="SIZES" tags="XS . SM . MD">
    <box direction="row" align="center" gap={space[3]}>
      {#each sizes as size}
        <Segmented items={spaces} bind:selected={tabs.shape} {size} />
      {/each}
      <label {...text.caption}>one binding, three steps</label>
    </box>
  </DemoPanel>
</SectionFrame>
