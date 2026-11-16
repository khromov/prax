<!-- The menus section's context menu (section_controls buildContextMenu and buildContextMenuItems): a zone that a
     secondary click opens the menu on, whose items are verbs, a row of tag colors, a check item and a destructive
     verb, beside a readout of what the menu chose. -->
<script>
    import DemoPanel from '../../parts/DemoPanel.svelte';
    import { menu } from '../../state/menus.svelte.js';
    import { space, text } from '../../tokens.js';

    // The verbs of the context menu.
    const editVerbs = [
        { label: 'Cut', shortcut: 'Cmd X' },
        { label: 'Copy', shortcut: 'Cmd C' },
        { label: 'Paste', shortcut: 'Cmd V' },
    ];
    const rowVerbs = [
        { label: 'Duplicate', shortcut: 'Cmd D' },
        { label: 'Rename', shortcut: 'F2' },
    ];

    // The colors of the palette that the row of tags offers.
    const tags = ['accent', 'success', 'warning', 'danger', 'axis-w'];
</script>

<DemoPanel title="CONTEXT MENU" tags="RIGHT-CLICK">
  <box direction="row" align="start" gap={space[4]}>
    <panel background="viewport" direction="row" justify="center" align="center" width={220} height={140} flex="fixed">
      <label {...text.monoSecondary}>RIGHT-CLICK ANYWHERE</label>
      <context-menu min-width={220}>
        <menu-label>EDIT</menu-label>
        {#each editVerbs as verb}
          <menu-item shortcut={verb.shortcut} onclick={() => (menu.verb = verb.label)}>{verb.label}</menu-item>
        {/each}
        <menu-separator></menu-separator>
        <menu-label>TAG</menu-label>
        <box direction="row" gap={space[2]} padding="{space[1]} {space[3]}">
          {#each tags as tag}
            <selectable selected={menu.tag === tag} padding={space[1]} onclick={() => (menu.tag = tag)}>
              <swatch color={tag} size={space[5]}></swatch>
            </selectable>
          {/each}
        </box>
        <menu-separator></menu-separator>
        <menu-item checked={menu.wrap} onclick={() => (menu.wrap = !menu.wrap)}>Word Wrap</menu-item>
        <menu-separator></menu-separator>
        {#each rowVerbs as verb}
          <menu-item shortcut={verb.shortcut} onclick={() => (menu.verb = verb.label)}>{verb.label}</menu-item>
        {/each}
        <menu-separator></menu-separator>
        <menu-item shortcut="Del" variant="danger" onclick={() => (menu.verb = 'Delete')}>Delete</menu-item>
      </context-menu>
    </panel>

    <!-- A press outside the menu closes it, and reaches this button as well. -->
    <box align="start" gap={space[2]}>
      <button onclick={() => ++menu.outsideClicks}>Not A Menu Item</button>
      <label {...text.monoMuted}>clicks: {menu.outsideClicks}</label>
      <label {...text.monoSecondary}>{menu.verb || 'no verb chosen yet'}</label>
      <box direction="row" gap={space[2]}>
        <swatch color={menu.tag} size={space[5]}></swatch>
        <label {...text.monoMuted}>{menu.wrap ? 'wrap: on' : 'wrap: off'}</label>
      </box>
    </box>
  </box>
</DemoPanel>
