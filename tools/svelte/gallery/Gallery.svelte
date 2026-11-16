<!-- The whole gallery (examples/studio_gallery/gallery_view.cpp buildGallery): the sidebar's table of contents beside
     the scrolling column of the sections. The sidebar marks the section that the content was scrolled to, which is the
     last whose top is near enough to the content's top, and brings a section into view when its row is activated. A
     widget tells where it was laid out with its bounds event, which is what the example reads from the tree. -->
<script>
    import { setTheme } from 'prax-svelte';
    import { sections } from './sections/index.js';
    import { height, space, text } from './tokens.js';

    // How far past the top of the content a section's top may be and still be the section the sidebar marks, in px.
    const activeSlack = 80;

    // The width of the sidebar, in px.
    const sidebarWidth = 240;

    // The sidebar's table of contents, which is also the order of the sections on the page. An entry that begins a
    // group has the group's heading.
    const entries = [
        { group: 'FOUNDATIONS', label: 'Principles', number: '00', name: 'principles' },
        { label: 'Color', number: '01', name: 'color' },
        { label: 'Type', number: '02', name: 'type' },
        { label: 'Space & Grid', number: '03', name: 'space' },
        { label: 'Icons', number: '04', name: 'icons' },
        { group: 'CONTROLS', label: 'Buttons', number: '05', name: 'buttons' },
        { label: 'Inputs', number: '06', name: 'inputs' },
        { label: 'Toggles', number: '07', name: 'toggles' },
        { label: 'Tabs & Segmented', number: '08', name: 'tabs' },
        { label: 'Menus & Popovers', number: '09', name: 'menus' },
        { group: 'COMPONENTS', label: 'Tree View', number: '10', name: 'tree' },
        { label: 'Inspector Rows', number: '11', name: 'inspector' },
        { label: 'Toolbar', number: '12', name: 'toolbar' },
        { label: 'Tables', number: '13', name: 'tables' },
        { label: 'Tags & Badges', number: '14', name: 'tags' },
        { group: 'FEEDBACK', label: 'Progress', number: '15', name: 'progress' },
        { label: 'Tooltips', number: '16', name: 'tooltips' },
        { label: 'Dialogs', number: '17', name: 'dialogs' },
        { label: 'Status & Console', number: '18', name: 'status' },
        { group: 'DATA', label: 'Collections', number: '19', name: 'collections' },
        { label: 'Timeline', number: '20', name: 'timeline' },
        { label: 'Images', number: '21', name: 'images' },
        { label: 'Node Graph', number: '22', name: 'graph' },
        { group: 'DIAGNOSTICS', label: 'Duplicate Keys', number: '23', name: 'keys' },
    ];

    // The theme that the gallery shows the kit in, which the host gives the kit as the example's window gives its kit
    // the palette that the sidebar chose.
    const themes = ['System', 'Dark', 'Light'];
    let theme = $state(0);
    $effect(() => setTheme(themes[theme].toLowerCase()));

    // The top of the content's view and each section's box at the last layout, and how many times each section's row
    // was activated, which is the value that brings the section into view again.
    let viewTop = $state(0);
    let boxes = $state(entries.map(() => null));
    let reveals = $state(entries.map(() => 0));

    const active = $derived.by(() => {
        let found = 0;
        boxes.forEach((box, index) => {
            if (box !== null && box.bottom > box.top && box.top - viewTop <= activeSlack)
                found = index;
        });
        return found;
    });
</script>

<box direction="row" flex="fill">
  <panel background="app" rules="right" overflow="clip" width={sidebarWidth} flex="fixed">
    <box gap={space[3]} padding="{space[5]} {space[4]} {space[5]} {space[4]}" flex="fixed">
      <label {...text.captionFaint}>PRAX DESIGN SYSTEM</label>
      <segmented size="sm">
        {#each themes as name, index (name)}
          <segment selected={theme === index} onclick={() => (theme = index)}>{name}</segment>
        {/each}
      </segmented>
    </box>
    <box overflow="scroll-y" flex="fill">
      {#each entries as entry, index (entry.name)}
        {#if entry.group}
          <box padding="{space[6]} {space[4]} {space[2]} {space[4]}" flex="fixed">
            <label {...text.captionFaint}>{entry.group}</label>
          </box>
        {/if}
        <selectable selected={active === index} direction="row" align="center" gap={space[3]} padding="0 {space[4]}"
            height={height.sm} flex="fixed" onclick={() => (reveals[index] += 1)}>
          <label tone={active === index ? 'primary' : 'secondary'} wrap="clip" flex="fill">{entry.label}</label>
          <label {...(active === index ? text.metaAccent : text.meta)}>{entry.number}</label>
        </selectable>
      {/each}
    </box>
  </panel>
  <box overflow="scroll-y" gap={space[16]} padding="{space[10] * 2} {space[12]} {space[16] * 3} {space[12]}" flex="fill"
      onbounds={(event) => (viewTop = event.values[1])}>
    {#each entries as entry, index (entry.name)}
      {@const Section = sections[entry.name]}
      <box gap={space[3]} reveal={reveals[index] || null}
          onbounds={(event) => (boxes[index] = { top: event.values[1], bottom: event.values[3] })}>
        <Section />
      </box>
    {/each}
  </box>
</box>
