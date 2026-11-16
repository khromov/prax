<!-- The gallery's sectionPrinciples (examples/studio_gallery/section_foundations.cpp). -->
<script>
    import Card from '../parts/Card.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    const principles = [
        {
            number: '01',
            title: 'Age well',
            body: 'Neutral panels, zero radius, restrained color. What ships today still reads the same in year five.',
        },
        {
            number: '02',
            title: 'Density without noise',
            body: 'A four-pixel grid and 22 to 26 pixel controls. Every pixel earns its place.',
        },
        {
            number: '03',
            title: 'Paths of discovery',
            body: 'Progressive disclosure, a chord on every action, and search wherever a list grows long.',
        },
        {
            number: '04',
            title: 'Legible structure',
            body: 'Monospaced ids and addresses, predictable node structure. A reader reasons about it.',
        },
        {
            number: '05',
            title: 'Indie to studio',
            body: 'The same panels carry a solo project and a two hundred person team.',
        },
        { number: '06', title: 'Full parity in light', body: 'Dark is the default. Light is never a downgrade.' },
    ];
</script>

<SectionFrame number="00" title="Principles" lede="Six commitments the system answers to.">
  <Card>
    <Grid items={principles} columns={3} ruled>
      {#snippet cell(principle)}
        <box gap={space[3]} padding={space[5]}>
          <label {...text.captionAccent}>{principle.number}</label>
          <label {...text.strong}>{principle.title}</label>
          <label {...text.bodySecondary}>{principle.body}</label>
        </box>
      {/snippet}
    </Grid>
  </Card>
</SectionFrame>
