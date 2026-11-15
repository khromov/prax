<!-- One section alone on the gallery's page: the scrolling column of the gallery's content and the box that the gallery
     gives each section (examples/studio_gallery/gallery_view.cpp), with no sidebar beside it. It is what the host
     declares for `--native gallery:<section>`. -->
<script>
    import { space } from './tokens.js';

    let { children } = $props();
</script>

<box overflow="scroll-y" gap={space[16]} padding="{space[10] * 2} {space[12]} {space[16] * 3} {space[12]}" flex="fill">
  <box gap={space[3]}>
    {@render children()}
  </box>
</box>
