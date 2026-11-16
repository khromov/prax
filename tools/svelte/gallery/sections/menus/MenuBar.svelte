<!-- The menus section's menu bar (section_controls buildMenuBar). Every item opens the file menu, under the item's own
     name. -->
<script>
    import DemoPanel from '../../parts/DemoPanel.svelte';

    const labels = ['File', 'Edit', 'View', 'Build', 'Window', 'Help'];

    // The commands of the file menu.
    const fileMenu = [
        { label: 'New Scene', shortcut: 'Cmd N' },
        { label: 'Open Scene...', shortcut: 'Cmd O' },
        { label: 'Save', shortcut: 'Cmd S' },
        { label: 'Save As...', shortcut: 'Shift Cmd S', disabled: true },
        { label: 'Build and Run', shortcut: 'Cmd B', separatorBefore: true },
    ];
</script>

<DemoPanel title="MENU BAR" tags="FILE OPENS" bare>
  <menu-bar>
    {#each labels as label}
      <menu-bar-item>{label}<popover>
          <menu min-width={240}>
            <menu-label>{label}</menu-label>
            {#each fileMenu as row}
              {#if row.separatorBefore}
                <menu-separator></menu-separator>
              {/if}
              <menu-item shortcut={row.shortcut} disabled={row.disabled}>{row.label}</menu-item>
            {/each}
          </menu>
        </popover>
      </menu-bar-item>
    {/each}
  </menu-bar>
</DemoPanel>
