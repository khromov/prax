<!-- The gallery's sectionTables (examples/studio_gallery/section_components.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { AssetSort, assets, selectionApply, selectionHolds, sortAssetOrder } from '../state/assets.js';

    const columns = [
        { label: 'NAME', width: 140, flex: 'fill', sortable: true },
        { label: 'TYPE', width: 110, sortable: true },
        { label: 'SIZE', width: 90, align: 'end', sortable: true },
        { label: 'MODIFIED', width: 110, sortable: true },
    ];

    // Fixed columns wider than the table: it scrolls in both axes, and the header follows the columns sideways while it
    // stays over the rows.
    const wide = [
        { label: 'NAME', width: 220 },
        { label: 'TYPE', width: 140 },
        { label: 'SIZE', width: 120, align: 'end' },
        { label: 'MODIFIED', width: 160 },
    ];

    const description =
        'Click a heading to sort by its column and again to reverse it; click, Shift-click and Cmd-click rows ' +
        'to select them. The name column fills the room that the fixed columns leave.';

    // The asset table's order and selection. The selection holds assets, by their index.
    let sort = $state({ column: AssetSort.name, descending: false });
    let cursor = $state({ anchor: 1, lead: 1 });
    let selection = $state([1]);

    const order = $derived(sortAssetOrder(assets, sort.column, !sort.descending));

    function sorted(event) {
        sort = { column: event.values[0], descending: event.values[1] === 1 };
    }

    function selected(event) {
        const [first, count, lead, anchor] = event.values;
        cursor = { anchor, lead };
        selectionApply(selection, { op: event.text, first, count }, order);
    }
</script>

<!-- The cells of an asset's row: its name by its icon, its type, its size and when it was modified. -->
{#snippet assetCells(asset)}
  <table-cell column={0} icon={asset.icon} role="body-small">{asset.name}</table-cell>
  <table-cell column={1} role="mono" tone="secondary">{asset.type}</table-cell>
  <table-cell column={2} role="mono" tone="secondary">{asset.sizeText}</table-cell>
  <table-cell column={3} role="mono" tone="muted">{asset.modified}</table-cell>
{/snippet}

<SectionFrame number="13" title="Tables" lede="Dense rows: a header that sorts, rows that select." {description}>
  <DemoPanel title="ASSET BROWSER" tags="{assets.length} ROWS" bare>
    <table selectable anchor={cursor.anchor} lead={cursor.lead} sort-column={sort.column}
        sort-descending={sort.descending} max-height={260} flex="fixed" onsort={sorted} onselect={selected}>
      {#each columns as column (column.label)}
        <table-column width={column.width} flex={column.flex} align={column.align}
            sortable={column.sortable}>{column.label}</table-column>
      {/each}
      {#each order as item (item)}
        <table-row selected={selectionHolds(selection, item)}>{@render assetCells(assets[item])}</table-row>
      {/each}
    </table>
  </DemoPanel>
  <!-- The panel is as wide as the table, which is narrower than its columns. -->
  <box direction="row" align="start">
    <DemoPanel title="OVERFLOWING COLUMNS" tags="SCROLLS BOTH AXES" bare>
      <table height={110} max-width={380} flex="fixed">
        {#each wide as column (column.label)}
          <table-column width={column.width} align={column.align}>{column.label}</table-column>
        {/each}
        {#each assets as asset (asset.name)}
          <table-row>{@render assetCells(asset)}</table-row>
        {/each}
      </table>
    </DemoPanel>
  </box>
</SectionFrame>
