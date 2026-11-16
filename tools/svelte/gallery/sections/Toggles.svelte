<!-- The gallery's sectionToggles (examples/studio_gallery/section_controls.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { toggles } from '../state/toggles.svelte.js';
    import { space } from '../tokens.js';

    // Each names the flag of the model that it reads and flips.
    const checks = [
        { value: 'castShadows', label: 'Cast Shadows' },
        { value: 'occlusionCull', label: 'Occlusion Cull' },
        { value: 'children', label: 'Children' },
        { value: 'locked', label: 'Locked' },
    ];

    const shadings = ['Unlit', 'Lambert', 'PBR Standard', 'Toon'];

    const switches = [
        { value: 'physics', label: 'Physics Simulation' },
        { value: 'snap', label: 'Snap to Grid' },
        { value: 'gizmos', label: 'Show Gizmos' },
    ];
</script>

{#snippet checkboxes()}
  <DemoPanel title="CHECKBOXES" tags="LIVE">
    {#each checks as check}
      <checkbox value={toggles[check.value]} onchange={(event) => (toggles[check.value] = event.value !== 0)}>
        {check.label}
      </checkbox>
    {/each}
  </DemoPanel>
{/snippet}

{#snippet radios()}
  <DemoPanel title="RADIO GROUP" tags="LIVE">
    <radio-group>
      {#each shadings as shading, index}
        <radio selected={toggles.shading === index} onclick={() => (toggles.shading = index)}>{shading}</radio>
      {/each}
    </radio-group>
  </DemoPanel>
{/snippet}

{#snippet toggleSwitches()}
  <DemoPanel title="TOGGLES" tags="LIVE">
    {#each switches as check}
      <toggle value={toggles[check.value]} onchange={(event) => (toggles[check.value] = event.value !== 0)}>
        {check.label}
      </toggle>
    {/each}
  </DemoPanel>
{/snippet}

<SectionFrame number="07" title="Checkboxes, Radios, Toggles" lede="Binary state, in every flavor.">
  <Grid items={[checkboxes, radios, toggleSwitches]} columns={3} gap={space[4]}>
    {#snippet cell(panel)}
      {@render panel()}
    {/snippet}
  </Grid>
</SectionFrame>
