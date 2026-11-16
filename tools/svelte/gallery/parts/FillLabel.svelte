<!-- A label that takes the room its parent leaves (gallery_parts fillLabel). It takes a label's attributes. -->
<script>
    let { children, ...style } = $props();
</script>

<label {...style} flex="fill">{@render children?.()}</label>
