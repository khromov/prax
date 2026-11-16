<!-- The menus section's command palette (section_controls buildPalette): a button that opens a palette of the commands
     that the query matches. -->
<script>
    import DemoPanel from '../../parts/DemoPanel.svelte';
    import { commandMatches, commands, palette } from '../../state/menus.svelte.js';
    import { space, text } from '../../tokens.js';

    // The most commands that the palette lists.
    const limit = 16;

    // The indices of the commands that the query matches.
    const shown = $derived.by(() => {
        const matches = [];
        for (let i = 0; i < commands.length && matches.length < limit; ++i) {
            if (commandMatches(commands[i], palette.query))
                matches.push(i);
        }
        return matches;
    });
</script>

<DemoPanel title="COMMAND PALETTE" tags="UP . DOWN . ENTER">
  <box direction="row" align="center" gap={space[3]}>
    <button icon="search">Open Palette...<command-palette prompt="Type a command" query={palette.query} searchable
          triggered onchange={(event) => (palette.query = event.text)}>
        {#each shown as index}
          {@const command = commands[index]}
          <command-item category={command.category} shortcut={command.shortcut} icon={command.icon}
              onclick={() => (palette.active = index)}>{command.label}</command-item>
        {/each}
        <command-palette-footer></command-palette-footer>
      </command-palette>
    </button>
    <label {...text.bodySecondary}>a modal at the top of the frame, over a list that the keys walk</label>
  </box>
</DemoPanel>
