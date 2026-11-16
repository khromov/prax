<!-- The gallery's sectionIcons (examples/studio_gallery/section_foundations.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    // The set's icons after `none`, in the order of PrKitIconKind (pr/kit_icon_kind.h), each by the name that
    // prKitIconName gives it. The example walks the set by number and asks the kit for each name, which a script
    // cannot, so an icon added to the set is added here.
    const icons = [
        'chevron-up', 'chevron-down', 'chevron-left', 'chevron-right', 'caret-right', 'arrow-left', 'arrow-right',
        'swap', 'undo', 'redo', 'cursor', 'move', 'rotate', 'scale', 'magnet', 'play', 'pause', 'stop', 'step',
        'step-over', 'step-into', 'step-out', 'restart', 'detach', 'close', 'plus', 'minus', 'search', 'filter',
        'settings', 'trash', 'more', 'dots-v', 'grid', 'eye', 'eye-off', 'lock', 'unlock', 'star', 'quote', 'hash',
        'ratio', 'asterisk', 'folder', 'folder-open', 'repository', 'worktree', 'branch', 'cube', 'sphere', 'light',
        'camera', 'script', 'audio', 'image', 'terrain', 'link', 'target', 'check', 'alert', 'info', 'spinner',
        'atlas', 'window-minimize', 'window-maximize', 'window-restore',
    ];

    const sizes = [10, 12, 14, 18, 24, 32];

    const description =
        'Every glyph the kit draws, in the order the set declares them. Each is a path, drawn at any size, and ' +
        'the draw atlas keeps its coverage, so a grid of them draws from the atlas after the first frame.';
</script>

<SectionFrame number="04" title="Icons" lede="One set, on a 24 by 24 grid, tinted by tone." {description}>
  <DemoPanel title="ICON SET" tags="EVERY GLYPH" bare>
    <Grid items={icons} columns={8} ruled>
      {#snippet cell(icon)}
        <box justify="center" align="center" overflow="clip" gap={space[3]} height={64} flex="fixed">
          <icon {icon} size={18} tone="secondary"></icon>
          <label role="meta" wrap="clip">{icon}</label>
        </box>
      {/snippet}
    </Grid>
  </DemoPanel>
  <DemoPanel title="SIZES" tags="ANY SIZE">
    <box direction="row" align="center" gap={space[6]}>
      {#each sizes as size (size)}
        <box align="center" gap={space[2]}>
          <icon icon="atlas" {size} tone="accent"></icon>
          <label {...text.meta}>{size}px</label>
        </box>
      {/each}
    </box>
    <box direction="row" gap={space[3]}>
      <label {...text.monoStrong}>prKitIconName</label>
      <label {...text.bodySecondary}>names every glyph the grid above loops over</label>
    </box>
  </DemoPanel>
</SectionFrame>
