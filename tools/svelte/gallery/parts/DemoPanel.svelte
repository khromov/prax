<!-- A panel of specimens (gallery_parts demoPanel): a card with a captioned header, tags at the header's end, and a
     body that holds the specimens. The body of a bare panel has no padding and no gap, so that its content reaches the
     card's edges. -->
<script>
    import { height, space, text } from '../tokens.js';
    import Card from './Card.svelte';

    let { title, tags = '', bare = false, children } = $props();
</script>

<Card>
  <panel background="header" rules="bottom" direction="row" align="center" gap={space[3]} padding="0 {space[5]}"
      height={height.sm} flex="fixed">
    <label {...text.caption}>{title}</label>
    {#if tags}
      <spacer></spacer>
      <badge>{tags}</badge>
    {/if}
  </panel>
  <box gap={bare ? 0 : space[3]} padding={bare ? 0 : space[5]}>{@render children?.()}</box>
</Card>
