<!-- A panel of specimens (gallery_parts demoPanel): a card with a captioned header, tags at the header's end, and a
     body that holds the specimens. The body of a bare panel has no padding and no gap, so that its content reaches the
     card's edges. The body is a box, which takes what else the panel is given. A caller listens there for the gestures
     of the body's controls (`oneditbegin`, `oneditchange`, `oneditcommit`), where the example sets an edit sink on the
     node that demoPanel returns. -->
<script>
    import { height, space, text } from '../tokens.js';
    import Card from './Card.svelte';

    let { title, tags = '', bare = false, children, ...body } = $props();
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
  <box {...body} gap={bare ? 0 : space[3]} padding={bare ? 0 : space[5]}>{@render children?.()}</box>
</Card>
