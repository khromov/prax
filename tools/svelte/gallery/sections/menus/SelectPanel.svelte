<!-- The menus section's selects (section_controls buildSelect): one of the component types, one of a list longer than
     the room it is given, and a disabled one of the first's choice. -->
<script>
    import DemoPanel from '../../parts/DemoPanel.svelte';
    import { animationClips, componentTypes, menu } from '../../state/menus.svelte.js';
    import { space, text } from '../../tokens.js';
    import Select from './Select.svelte';
</script>

<DemoPanel title="SELECT" tags="DROPDOWN">
  <box direction="row" align="center" gap={space[4]}>
    <Select labels={componentTypes} bind:selected={menu.component} width={220} flex="fixed" />
    <label {...text.bodySecondary}>Up and Down step the value while the list is closed</label>
  </box>
  <box direction="row" align="center" gap={space[4]}>
    <Select labels={animationClips} bind:selected={menu.clip} width={220} flex="fixed" />
    <label {...text.bodySecondary}>66 clips, clamped and scrolled</label>
  </box>
  <box direction="row" align="center" gap={space[4]}>
    <Select labels={componentTypes} bind:selected={menu.component} disabled width={220} flex="fixed" />
    <label {...text.bodySecondary}>a disabled select shows its value and takes no input</label>
  </box>
</DemoPanel>
