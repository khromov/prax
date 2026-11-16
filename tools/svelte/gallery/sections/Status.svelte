<!-- The gallery's sectionStatus (examples/studio_gallery/section_feedback.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { height, space } from '../tokens.js';

    // The status bar's plain items, each after a divider.
    const items = ['Level_01.pr', 'ENTITIES 248 . DRAW 1,412 . TRIS 2.4M', '60.00 fps'];

    const stats = [
        { label: 'FRAME', value: '16.6 ms', note: '60 fps target' },
        { label: 'DRAW CALLS', value: '1,412', note: '248 entities' },
        { label: 'GPU', value: '11.2 ms', note: 'of the 16.6 budget', noteTone: 'warning' },
        { label: 'ERRORS', value: '1', note: 'since the last build', noteTone: 'danger' },
    ];

    // The console's seeded lines.
    const log = [
        { time: '12:04:18.2', message: 'Scene Level_01 loaded', source: 'SceneManager', level: 'info' },
        { time: '12:04:18.3', message: 'Compiling 142 shaders', source: 'ShaderCache', level: 'info' },
        { time: '12:04:19.4', message: 'Compilation complete in 1.1 s', source: 'ShaderCache', level: 'info' },
        {
            time: '12:04:21.0',
            message: 'Texture env_diffuse.ktx2 has non-power-of-two dimensions',
            source: 'AssetImport',
            level: 'warning',
        },
        {
            time: '12:04:25.6',
            message: 'Entity Enemy_04 is missing a Rigidbody',
            source: 'PhysicsSystem',
            level: 'error',
        },
        {
            time: '12:04:25.7',
            message: 'LOD group reports no levels for tree_pine_03.mesh',
            source: 'Renderer',
            level: 'warning',
        },
        { time: '12:04:26.0', message: 'Autosave complete', source: 'Autosave', level: 'info' },
    ];

    const messages = [
        { message: 'Connected to the build server' },
        { message: 'Shader cache 96 percent warm' },
        { message: 'Package manifest older than the lockfile', level: 'warning' },
        { message: 'Upload failed: connection reset', level: 'error' },
    ];
</script>

<!-- An item of the status bar: a caption in a tone. -->
{#snippet status(content, tone)}
  <label role="caption" {tone}>{content}</label>
{/snippet}

<SectionFrame number="18" title="Status Bar & Console" lede="The two places the engine talks back.">
  <DemoPanel title="STATUS BAR" tags="BOTTOM CHROME" bare>
    <panel background="header" rules="top" direction="row" align="center" gap={space[4]} padding="0 {space[4]}"
        overflow="clip" height={height.sm} flex="fixed">
      <box direction="row" gap={space[2]}>
        <icon icon="check" size={10} tone="success"></icon>
        {@render status('BUILD OK', 'success')}
      </box>
      {#each items as item (item)}
        <divider></divider>
        {@render status(item, 'muted')}
      {/each}
      <spacer></spacer>
      {@render status('LINE 42 . COL 18', 'muted')}
      <divider></divider>
      <box direction="row" gap={space[2]}>
        <icon icon="alert" size={10} tone="warning"></icon>
        {@render status('2', 'warning')}
      </box>
      <box direction="row" gap={space[2]}>
        <icon icon="alert" size={10} tone="danger"></icon>
        {@render status('1', 'danger')}
      </box>
      <divider></divider>
      {@render status('UTF-8', 'muted')}
    </panel>
  </DemoPanel>

  <DemoPanel title="STAT TILES" tags="READOUTS">
    <box direction="row" align="stretch" gap={space[3]}>
      {#each stats as stat (stat.label)}
        <stat-tile value={stat.value} note={stat.note} note-tone={stat.noteTone}>{stat.label}</stat-tile>
      {/each}
    </box>
  </DemoPanel>

  <DemoPanel title="CONSOLE" tags="LOG LEVELS" bare>
    <panel background="panel" overflow="scroll-y" height={180} flex="fixed">
      {#each log as entry (entry.time)}
        <log-line time={entry.time} source={entry.source} level={entry.level}>{entry.message}</log-line>
      {/each}
    </panel>
  </DemoPanel>
  <DemoPanel title="COMPACT CONSOLE" tags="MESSAGE ONLY" bare>
    {#each messages as entry (entry.message)}
      <log-line level={entry.level}>{entry.message}</log-line>
    {/each}
  </DemoPanel>
</SectionFrame>
