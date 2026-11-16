<!-- The gallery's sectionProgress (examples/studio_gallery/section_feedback.cpp).

     The example moves its bake with the time of the round, which a script cannot read: a script has timers, and
     `performance.now()` is coarsened to a tenth of a millisecond. The bake here follows a clock of the section's own
     instead, which a timer moves a sixtieth of a second at a time. A host that runs sixty frames a second runs the
     timer once before each, so the clock keeps the frames' time. A frame that runs a second round, as one that
     routes a press does, shows the example's bake a frame ahead of this one, since the timer ran once for it. -->
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

    // The time between two ticks of the section's clock, in seconds and in the whole milliseconds of a timer.
    const tick = 1 / 60;
    const tickMilliseconds = 16;

    // The section's clock, in seconds: the time of the last tick, which is 0 before the first.
    let time = 0;

    // The example moves the bake after it has declared the bar, so each of its frames shows the bake of the frame
    // before, and its first shows the bake that the model starts with. A tick therefore shows the time of the tick
    // before it.
    function advance() {
        if (progress.running && time > 0) {
            const turns = time * 0.25;
            progress.bake = Math.fround(turns - Math.trunc(turns));
        }
        time += tick;
    }

    $effect(() => {
        const timer = setInterval(advance, tickMilliseconds);
        return () => clearInterval(timer);
    });

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
      <progress value={progress.bake} flex="fill"></progress>
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
