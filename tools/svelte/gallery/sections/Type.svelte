<!-- The gallery's sectionType (examples/studio_gallery/section_foundations.cpp). -->
<script>
    import CaptionRow from '../parts/CaptionRow.svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    const scale = [
        { key: 'Display', meta: '40 / 600', sample: 'Hold it in your head', role: 'display', tone: 'primary' },
        { key: 'Title', meta: '22 / 500', sample: 'Section Title', role: 'title', tone: 'primary' },
        { key: 'Heading', meta: '16 / 500', sample: 'Panel Heading', role: 'heading', tone: 'primary' },
        {
            key: 'Body',
            meta: '12 / 400',
            sample: 'The quick brown fox jumps over the lazy dog. 0123456789',
            role: 'body',
            tone: 'primary',
        },
        {
            key: 'BodySmall',
            meta: '11 / 400',
            sample: 'Inspector row labels at eleven pixels.',
            role: 'body-small',
            tone: 'primary',
        },
        { key: 'Strong', meta: '12 / 500', sample: 'Emphasis inside body text', role: 'strong', tone: 'primary' },
        {
            key: 'BodyItalic',
            meta: '12 / 400 . ITALIC',
            sample: 'A preview tab, a derived value',
            role: 'body-italic',
            tone: 'primary',
        },
        { key: 'Caption', meta: '10 / 500 . MONO', sample: 'UPPERCASE . LABELS . META', role: 'caption', tone: 'muted' },
        { key: 'Meta', meta: '10 / 400 . MONO', sample: '42 items . view . 2026-08-02', role: 'meta', tone: 'faint' },
        {
            key: 'Mono',
            meta: '11 / 400 . MONO',
            sample: 'entity_id: 0x7ff8_a2b0 . pos = (1.234, 0.000, -4.500)',
            role: 'mono',
            tone: 'muted',
        },
        { key: 'MonoStrong', meta: '11 / 500 . MONO', sample: 'player_base.mesh', role: 'mono-strong', tone: 'primary' },
        { key: 'MonoNum', meta: '22 / 500 . MONO', sample: '128 . 2048 . 60.00fps', role: 'mono-num', tone: 'primary' },
    ];

    const tones = ['Primary', 'Secondary', 'Muted', 'Faint', 'Accent', 'Success', 'Warning', 'Danger'];

    const description =
        'The sans face carries everything a person wrote; the monospaced face carries everything a machine ' +
        'produced -- coordinates, hashes, timestamps, chords and log lines. A label names a role and a tone, ' +
        'never a face, a size or a color.';
</script>

<SectionFrame number="02" title="Type" lede="One family, two voices." {description}>
  <DemoPanel title="TYPE SCALE" tags="12 ROLES">
    {#each scale as step (step.key)}
      <panel rules="bottom" direction="row" align="baseline" gap={space[5]} padding="{space[2]} 0" flex="fixed">
        <box direction="row" width={100} flex="fixed"><label {...text.caption}>{step.key}</label></box>
        <box direction="row" width={110} flex="fixed"><label {...text.meta}>{step.meta}</label></box>
        <label role={step.role} tone={step.tone} wrap="clip" flex="fill">{step.sample}</label>
      </panel>
    {/each}
  </DemoPanel>
  <DemoPanel title="TONE" tags="EMPHASIS . STATE">
    {#each tones as tone (tone)}
      <CaptionRow caption={tone} labelWidth={100}>
        <label tone={tone.toLowerCase()} wrap="clip" flex="fill">The quick brown fox jumps over the lazy dog.</label>
      </CaptionRow>
    {/each}
  </DemoPanel>
</SectionFrame>
