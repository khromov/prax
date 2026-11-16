<!-- The gallery's sectionCollections (examples/studio_gallery/section_data.cpp).

     The example declares whichever of its three views shows at one place, so they are one scroll container, and a
     view shows where the view before it was scrolled to. The list and the grid are one `list-view` here, which keeps
     that. The tree is a `tree-view`, a node of another kind, so it has a scroll offset of its own. -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import {
        collectionAssetOf, collectionGroupOf, collectionTreeKey, collectionsTreeOrder, groupCount, groupSize,
        seedCollections, selectionApply,
    } from '../state/collections.js';
    import { height, space, text } from '../tokens.js';

    // The collection's row height and viewport height, and the least width and the height of a grid's tile, in px.
    const rowHeight = 32;
    const viewHeight = 360;
    const tileWidth = 112;
    const tileHeight = 104;

    // The views that the segmented control chooses between.
    const CollectionView = { list: 0, grid: 1, tree: 2 };
    const modes = ['List', 'Grid', 'Tree'];

    const verbs = ['Open', 'Reveal in Finder'];

    const description =
        'The set is five thousand generated assets, and only the rows the viewport shows are built, a dozen or ' +
        'so of them at a time. Click to select, Shift-click for a range, Cmd or Ctrl-click to add or remove ' +
        'one; the arrow keys move the lead and Shift extends from the anchor. The grid shows the same assets ' +
        "as tiles, as many to a row as fit, and shares the list's selection. The tree reaches them through " +
        'their groups, where Right opens a group and steps into it and Left closes it.';

    // The collection's assets, each view's selection and cursor, and what its context menu did. A selection holds
    // assets, and the tree's holds the keys of its rows (collectionTreeKey).
    const assets = seedCollections();
    let selection = $state.raw(new Set());
    let treeSelection = $state.raw(new Set());
    let groupOpen = $state(Array.from({ length: groupCount }, () => false));
    let verb = $state('');
    let cursor = $state.raw({ anchor: 0, lead: 0 });
    let treeCursor = $state.raw({ anchor: 0, lead: 0 });
    let viewMode = $state(CollectionView.list);

    const tree = $derived(viewMode === CollectionView.tree);
    const held = $derived(tree ? treeSelection.size : selection.size);

    // The keys of the tree's visible rows, which its view's events count.
    const treeKeys = $derived(collectionsTreeOrder(groupOpen));

    // Apply the edit that a view's `select` tells to the view's selection, and keep the cursor that the view left, so
    // that the view has both again once another view has shown in its place.
    function selectAssets(event) {
        const [first, count, lead, anchor] = event.values;
        selection = selectionApply(selection, event.text, first, count, null);
        cursor = { anchor, lead };
    }
    function selectTreeRows(event) {
        const [first, count, lead, anchor] = event.values;
        treeSelection = selectionApply(treeSelection, event.text, first, count, treeKeys);
        treeCursor = { anchor, lead };
    }

    function foldGroup(event) {
        const [row, open] = event.values;
        groupOpen[collectionGroupOf(treeKeys[row])] = open === 1;
    }
</script>

<!-- The context menu of whichever view shows, whose items set the verb that the toolbar shows. -->
{#snippet menu()}
  <context-menu min-width={200}>
    {#each verbs as item (item)}
      <menu-item onclick={() => (verb = item)}>{item}</menu-item>
    {/each}
    <menu-separator></menu-separator>
    <menu-item variant="danger" onclick={() => (verb = 'Delete')}>Delete</menu-item>
  </context-menu>
{/snippet}

<SectionFrame number="19" title="Collections" lede="Virtualized multi-select over five thousand assets." {description}>
  <DemoPanel title="ASSET BROWSER" tags="LIST . GRID . TREE" bare>
    <panel rules="bottom" direction="row" align="center" gap={space[3]} padding="0 {space[4]}" height={height.md}
        flex="fixed">
      <segmented size="sm">
        {#each modes as mode, index (mode)}
          <segment selected={viewMode === index} onclick={() => (viewMode = index)}>{mode}</segment>
        {/each}
      </segmented>
      <spacer></spacer>
      <label {...text.monoMuted}>{held} of {assets.length} selected</label>
      {#if verb}
        <label tone="secondary" wrap="nowrap">{verb}</label>
      {/if}
    </panel>

    {#if tree}
      <tree-view height={viewHeight} flex="fixed" selectable anchor={treeCursor.anchor} lead={treeCursor.lead}
          onselect={selectTreeRows} onfold={foldGroup}>
        {#each groupOpen as open, group}
          <tree-row depth={0} has-children {open} selected={treeSelection.has(collectionTreeKey(group))}>
            <row-icon icon={open ? 'folder-open' : 'folder'}></row-icon>
            <row-label tone="primary">Group {group}</row-label>
            <row-meta>{groupSize}</row-meta>
          </tree-row>
          {#if open}
            {#each { length: groupSize } as _, child}
              {@const key = collectionTreeKey(group, child)}
              {@const asset = assets[collectionAssetOf(key)]}
              <tree-row depth={1} selected={treeSelection.has(key)}>
                <row-icon icon={asset.icon}></row-icon>
                <row-label tone="primary">{asset.name}</row-label>
                <row-meta>{asset.type}</row-meta>
              </tree-row>
            {/each}
          {/if}
        {/each}
        {@render menu()}
      </tree-view>
    {:else}
      {@const grid = viewMode === CollectionView.grid}
      <!-- The grid's tiles share the list's view, as they share its selection. -->
      <list-view row-height={grid ? tileHeight : rowHeight} tile-width={grid ? tileWidth : null} height={viewHeight}
          flex="fixed" selectable anchor={cursor.anchor} lead={cursor.lead} onselect={selectAssets}>
        {#if grid}
          {#each assets as asset, index}
            <list-tile selected={selection.has(index)}>
              <tile-icon icon={asset.icon} tint={asset.tint}></tile-icon>
              <tile-label>{asset.name}</tile-label>
            </list-tile>
          {/each}
        {:else}
          {#each assets as asset, index}
            <list-row selected={selection.has(index)}>
              <row-icon icon={asset.icon}></row-icon>
              <row-label tone="primary">{asset.name}</row-label>
              <row-meta>{asset.sizeText}</row-meta>
              <box direction="row" justify="end" width={60} flex="fixed">
                <label {...text.monoFaint}>{asset.type}</label>
              </box>
            </list-row>
          {/each}
        {/if}
        {@render menu()}
      </list-view>
    {/if}
  </DemoPanel>

  <DemoPanel title="EMPTY STATE" tags="NO RESULTS" bare>
    <panel background="panel" height={140} flex="fixed">
      <empty-state icon="search" hint="Clear the filter to see every asset again">
        No assets match the filter
      </empty-state>
    </panel>
  </DemoPanel>
</SectionFrame>
