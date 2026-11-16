<!-- The gallery's sectionProgress (examples/studio_gallery/section_feedback.cpp).

     The example moves its bake with the time of each round, which the bake's bar hears here (`round`). The section
     asks for no frames of its own: the bake moves in the frames that the moving widgets beside it ask for, as the
     example's does. -->
<script>
    import CaptionRow from '../parts/CaptionRow.svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';
    import SpecimenRow from './feedback/SpecimenRow.svelte';
    import { bodySmall } from './feedback/styles.js';

    // The values that the progress specimens show (gallery_state.h ProgressState). The bake is a float, as it is in
    // the example, so that it shows the same fraction and the same percent.
    const progress = $state({ bake: Math.fround(0.32), memory: 0.62, budget: 11.2, running: true });

    const fractions = [0, 0.25, 0.5, 0.75, 1];
    const sizes = [12, 16, 24, 32];

    // The time of the last round that the bar heard of, in seconds, or null before the first.
    let last = null;

    // The example moves the bake after it has declared the bar, so each of its rounds shows the time of the round
    // before, and its first shows the bake that the model starts with. A script hears of a round before the round is
    // declared, so the bake takes the time of the round before here too. A frame's second round, which a frame that
    // routes a press runs, then shows the frame's own time.
    function follow(event) {
        if (progress.running && last !== null) {
            const turns = last * 0.25;
            progress.bake = Math.fround(turns - Math.trunc(turns));
        }
        last = event.value;
    }

    // Returns the whole number nearest `value`, which is not negative, and the even one of two that are as near,
    // which is how the example's format writes a number with no decimals.
    function nearestEven(value) {
        const whole = Math.floor(value);
        const rest = value - whole;
        return rest > 0.5 || (rest === 0.5 && whole % 2 === 1) ? whole + 1 : whole;
    }

    // The example multiplies the bake by 100 as a float.
    const percent = $derived(nearestEven(Math.fround(progress.bake * 100)) + '%');

    const description =
        'A determinate bar states a fraction, an indeterminate one states only that work is running, and a meter ' +
        'lights the segments its value covers. Each of the moving ones asks for the frames that move it.';
</script>

<SectionFrame number="15" title="Progress" lede="Work in flight, and a reading against a budget." {description}>
  <DemoPanel title="PROGRESS" tags="DETERMINATE . INDETERMINATE">
    <CaptionRow caption="BAKE" labelWidth={92}>
      <progress value={progress.bake} flex="fill" onround={follow}></progress>
      <label {...text.monoMuted}>{percent}</label>
    </CaptionRow>
    <CaptionRow caption="SCANNING" labelWidth={92}>
      <progress indeterminate flex="fill"></progress>
      <label {...bodySmall}>no fraction to state</label>
    </CaptionRow>
    <SpecimenRow gap={space[3]}>
      {#each fractions as fraction (fraction)}
        <progress value={fraction} width={80} flex="fixed"></progress>
      {/each}
    </SpecimenRow>
  </DemoPanel>
  <DemoPanel title="METERS" tags="SEGMENTS . TONE">
    <CaptionRow caption="MEMORY" labelWidth={92}>
      <meter value={progress.memory} tone="accent" flex="fill"></meter>
    </CaptionRow>
    <CaptionRow caption="FRAME MS" labelWidth={92}>
      <meter value={progress.budget} low={0} high={16.6} tone="warning" flex="fill"></meter>
    </CaptionRow>
    <CaptionRow caption="ERRORS" labelWidth={92}>
      <meter value={1} low={0} high={8} tone="danger" flex="fill"></meter>
    </CaptionRow>
  </DemoPanel>
  <DemoPanel title="SPINNER" tags="SIZES">
    <SpecimenRow gap={space[6]}>
      {#each sizes as size (size)}
        <spinner {size}></spinner>
      {/each}
      <label {...text.bodySecondary}>an arc that turns while the work runs</label>
    </SpecimenRow>
  </DemoPanel>
</SectionFrame>
