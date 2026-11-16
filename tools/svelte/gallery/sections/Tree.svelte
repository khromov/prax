<!-- The gallery's sectionTree (examples/studio_gallery/section_components.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { findTreeNode, moveTreeNode, seedTree, treeOrder } from '../state/tree.js';

    // The kind of the payload that a row of the tree carries.
    const dragTreeRow = 1;

    const description =
        'Click a row and Up and Down walk the tree, Left and Right fold and unfold it. A row carries an ' +
        'optional status in place of its glyph, and a drag lands before, after or into the row under the pointer.';

    // The scene tree, and the key of its selected node.
    let nodes = $state(seedTree());
    let selected = $state(6);

    // The visible rows, in display order.
    const rows = $derived(treeOrder(nodes));

    // The node that the ghost of a dragged row names. The gallery reads the payload in flight, shows the ghost only for
    // a row of this tree, and names the row that is dragged. A tree view tells a script which row was dragged only once
    // it is dropped, and a ghost shows for a payload of any kind. So this names the selected node, which is the dragged
    // one only if the drag starts on the selected row.
    const carried = $derived(findTreeNode(nodes, selected));

    function folded(event) {
        const [row, open] = event.values;
        if (row < rows.length)
            rows[row].open = open === 1;
    }

    function selectionEdited(event) {
        const lead = event.values[2];
        if (lead < rows.length)
            selected = rows[lead].key;
    }

    // Moves the dragged row before or after the row that it was dropped on, or else into it.
    function dropped(event) {
        const [row, dragged] = event.values;
        if (row >= rows.length || dragged < 0 || dragged >= rows.length)
            return;
        let placement = 'inside';
        if (event.text === 'before' || event.text === 'after')
            placement = event.text;
        moveTreeNode(nodes, rows[dragged].key, rows[row].key, placement);
    }
</script>

<SectionFrame number="10" title="Tree View" lede="Expand, select, hide, drag to reparent." {description}>
  <DemoPanel title="SCENE HIERARCHY" tags="LIVE . DRAG TO REPARENT" bare>
    <tree-view selectable drag-kind={dragTreeRow} height={300} flex="fixed" onselect={selectionEdited} onfold={folded}
        ondrop={dropped}>
      {#each rows as node (node.key)}
        <tree-row depth={node.depth} has-children={node.hasChildren} open={node.open} selected={selected === node.key}>
          <!-- A node with a status shows it in place of its icon. -->
          {#if node.badge}
            <status tone={node.badgeTone}>{node.badge}</status>
          {:else}
            <row-icon icon={node.icon}></row-icon>
          {/if}
          <row-label tone={node.hidden ? 'faint' : 'primary'}>{node.name}</row-label>
          {#if node.meta}
            <row-meta>{node.meta}</row-meta>
          {/if}
          <visibility-toggle hidden={node.hidden} revealed="hover"
              onclick={() => (node.hidden = !node.hidden)}></visibility-toggle>
        </tree-row>
      {/each}
    </tree-view>
  </DemoPanel>
  {#if carried !== null}
    <drag-ghost icon={carried.icon}>{carried.name}</drag-ghost>
  {/if}
</SectionFrame>
