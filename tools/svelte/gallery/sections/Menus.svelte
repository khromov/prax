<!-- The gallery's sectionMenus (examples/studio_gallery/section_controls.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { toggles } from '../state/toggles.svelte.js';
    import { space, text } from '../tokens.js';
    import ContextMenu from './menus/ContextMenu.svelte';
    import MenuBar from './menus/MenuBar.svelte';
    import Palette from './menus/Palette.svelte';
    import SelectPanel from './menus/SelectPanel.svelte';

    const description =
        'Every portal here places itself from the box its trigger last stood in, stays inside the ' +
        'frame, and takes itself down on Escape or on a press that lands outside it.';

    // The layers of the popover, each of which names the flag of the model that it reads and flips.
    const layers = [
        { value: 'castShadows', label: 'Shadows' },
        { value: 'gizmos', label: 'Gizmos' },
        { value: 'children', label: 'Children' },
    ];
</script>

<SectionFrame number="09" title="Menus & Popovers" lede="Application menus, context menus, a palette." {description}>
  <MenuBar />
  <SelectPanel />
  <DemoPanel title="POPOVER" tags="TRIGGER . FRAME">
    <box direction="row" align="center" gap={space[3]}>
      <button icon="chevron-down">Layers<popover triggered>
          <box gap={space[2]} padding={space[5]} width={220}>
            <label {...text.caption}>VISIBLE LAYERS</label>
            {#each layers as layer}
              <checkbox value={toggles[layer.value]} onchange={(event) => (toggles[layer.value] = event.value !== 0)}>
                {layer.label}
              </checkbox>
            {/each}
          </box>
        </popover>
      </button>
      <label {...text.bodySecondary}>a popover holds whatever the caller declares in it</label>
    </box>
  </DemoPanel>
  <ContextMenu />
  <Palette />
</SectionFrame>
