<!-- A segmented control over a choice (gallery_parts segmented): a segment for each of `items`, which has a `label`, an
     `icon`, or both. The segment at index `selected` is the chosen one, and activating a segment writes its index to
     `selected`, which a caller binds. It takes a segmented control's attributes, such as its `size`. -->
<script>
    let { items, selected = $bindable(), ...style } = $props();
</script>

<segmented {...style}>
  {#each items as item, index}
    <segment icon={item.icon} selected={selected === index} onclick={() => (selected = index)}>{item.label}</segment>
  {/each}
</segmented>
