<!-- A card (gallery_parts card): a panel on the panel background with a rule on every side. It takes a box's
     attributes, which lay out what it holds. -->
<script>
    let { children, ...box } = $props();
</script>

<panel background="panel" rules="all" {...box}>{@render children?.()}</panel>
