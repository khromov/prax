<!-- The gallery's sectionButtons (examples/studio_gallery/section_controls.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space } from '../tokens.js';

    // The tools of a tool row.
    const toolIcons = ['cursor', 'move', 'rotate', 'scale'];

    const sizes = [
        { size: 'xs', label: 'OK' },
        { size: 'sm', label: 'Save' },
        { size: 'md', label: 'Play' },
        { size: 'lg', label: 'Build Project' },
    ];

    const description =
        'Primary for the one dominant action of a look, secondary for the neutral options, ghost for ' +
        'low-weight and toolbar use, and danger for what destroys something.';
</script>

<SectionFrame number="05" title="Buttons" lede="Four variants across four height steps." {description}>
  <DemoPanel title="VARIANTS" tags="4 TYPES">
    <box direction="row" align="center" wrap gap={space[3]}>
      <button variant="primary">Import Asset</button>
      <button>Cancel</button>
      <button variant="ghost">Clear Selection</button>
      <button variant="danger">Delete Entity</button>
      <button disabled>Disabled</button>
    </box>
  </DemoPanel>
  <DemoPanel title="SIZES" tags="XS . SM . MD . LG">
    <box direction="row" align="center" gap={space[3]}>
      {#each sizes as step (step.size)}
        <button variant="primary" size={step.size}>{step.label}</button>
      {/each}
    </box>
  </DemoPanel>
  <DemoPanel title="ICON BUTTONS" tags="SQUARE . LABELLED">
    <box direction="row" align="center" gap={space[3]}>
      {#each toolIcons as icon (icon)}
        <button {icon}></button>
      {/each}
      <button icon="plus">Add Entity</button>
      <button icon="search">Search</button>
    </box>
  </DemoPanel>
  <DemoPanel title="TOOLTIPS" tags="HOVER . VERB . CHORD">
    <box direction="row" align="center" gap={space[3]}>
      <button icon="undo">Undo<tooltip shortcut="Cmd Z">Undo the last edit</tooltip></button>
      <button icon="redo"><tooltip shortcut="Cmd Shift Z">Redo the last undone edit</tooltip></button>
      <button icon="trash" variant="ghost" size="md"><tooltip shortcut="Del">Delete selection</tooltip></button>
    </box>
  </DemoPanel>
  <DemoPanel title="BUTTON GROUP" tags="SHARED BORDER">
    <box direction="row" align="center" gap={space[6]}>
      <button-group>
        {#each toolIcons as icon (icon)}
          <button {icon}></button>
        {/each}
      </button-group>
      <button-group>
        <button size="sm">Local</button>
        <button size="sm" selected>World</button>
      </button-group>
    </box>
  </DemoPanel>
</SectionFrame>
