<!-- A row of specimens a gap apart, centered across, which wraps (section_feedback.cpp specimenRow). -->
<script>
    let { gap, children } = $props();
</script>

<box direction="row" align="center" wrap {gap}>{@render children?.()}</box>
