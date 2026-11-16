<!-- A grid of equal columns (gallery_parts Grid): its items in rows of `columns` cells, a gap apart, or ruled apart
     with no gap. The last row is filled with empty cells, so that its cells are as wide as the others. `cell` renders
     an item in its cell. -->
<script>
    let { items, columns, gap = 0, ruled = false, cell } = $props();

    const rows = $derived.by(() => {
        const made = [];
        for (let start = 0; start < items.length; start += columns)
            made.push(Array.from({ length: columns }, (_, column) => items[start + column]));
        return made;
    });
</script>

{#each rows as row}
  <box direction="row" align="stretch" gap={ruled ? 0 : gap}>
    {#each row as item}
      <panel rules={ruled ? 'right bottom' : null} flex="fill">
        {#if item !== undefined}
          {@render cell(item)}
        {/if}
      </panel>
    {/each}
  </box>
{/each}
