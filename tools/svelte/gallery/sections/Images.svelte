<!-- The gallery's sectionImages (examples/studio_gallery/section_feedback.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { galleryPicture } from '../state/picture.js';
    import { space, text } from '../tokens.js';
    import Framed from './feedback/Framed.svelte';
    import { bodySmall } from './feedback/styles.js';

    const picture = galleryPicture();

    const specimens = [
        { title: 'FILL', tags: 'DEFAULT', fit: 'fill', caption: 'Stretched over the frame; the ratio is not kept.' },
        { title: 'CONTAIN', tags: 'FIT', fit: 'contain', caption: 'The whole picture inside the frame, letterboxed.' },
        {
            title: 'COVER',
            tags: 'CROP',
            fit: 'cover',
            caption: 'The frame filled edge to edge; the overflow is cropped.',
        },
        {
            title: 'SCALE DOWN',
            tags: 'FIT',
            fit: 'scale-down',
            caption: 'Contain, but never larger than the picture itself.',
        },
    ];

    // The place of a picture in its frame.
    const frame = { width: '100%', height: '100%', flex: 'fixed' };

    const aligns = [
        { name: 'START', align: 'start' },
        { name: 'CENTER', align: 'center' },
        { name: 'END', align: 'end' },
    ];

    const description =
        'A fit places the picture inside the box that layout already sized; it never sizes the box. The ' +
        'alignment slides the picture within the box, which moves the part of a cropped picture that shows.';
</script>

<SectionFrame number="21" title="Images" lede="One wide picture in four bounded frames." {description}>
  <Grid items={specimens} columns={4} gap={space[6]}>
    {#snippet cell(specimen)}
      <DemoPanel title={specimen.title} tags={specimen.tags}>
        <Framed><image pixels={picture} fit={specimen.fit} {...frame}></image></Framed>
        <label {...bodySmall}>{specimen.caption}</label>
      </DemoPanel>
    {/snippet}
  </Grid>

  <DemoPanel title="ALIGNMENT" tags="COVER">
    <box direction="row" align="start" gap={space[6]}>
      {#each aligns as cell (cell.name)}
        <box align="center" gap={space[2]}>
          <Framed><image pixels={picture} fit="cover" align-x={cell.align} {...frame}></image></Framed>
          <label {...text.caption}>{cell.name}</label>
        </box>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
