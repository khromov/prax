<!-- A row led by a caption in a column of a width (gallery_parts captionRow), which holds the specimen after it. -->
<script>
    import { space, text } from '../tokens.js';

    let { caption, labelWidth, children } = $props();
</script>

<box direction="row" align="center" gap={space[4]}>
  <box direction="row" width={labelWidth} flex="fixed"><label {...text.caption}>{caption}</label></box>
  {@render children?.()}
</box>
