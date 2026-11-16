<!-- A numbered section (gallery_parts sectionFrame): a header of its number, its title and, at the header's end, its
     lede, then its description, if any, in a column of a fixed measure, then the section's panels. -->
<script>
    import { space, text } from '../tokens.js';

    let { number, title, lede = '', description = '', children } = $props();

    // The width of a section's description, in px.
    const measure = 600;
</script>

<box gap={space[3]}>
  <panel rules="bottom" direction="row" align="baseline" gap={space[4]} padding="0 0 {space[6]} 0" flex="fixed">
    <label {...text.monoStrongFaint}>{number}</label>
    <label {...text.title}>{title}</label>
    {#if lede}
      <label {...text.lede} flex="fill">{lede}</label>
    {/if}
  </panel>
  {#if description}
    <box padding="{space[3]} 0 {space[5]} 0" max-width={measure}><label {...text.bodySecondary}>{description}</label></box>
  {/if}
  {@render children?.()}
</box>
