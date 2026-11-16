<!-- The gallery's sectionInspector (examples/studio_gallery/section_components.cpp). -->
<script>
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';

    // The kind of the payload that an asset chip carries.
    const dragAssetChip = 2;

    // The assets that the inspector's mesh field takes.
    const assetChips = [
        { name: 'player_base.mesh', icon: 'cube' },
        { name: 'enemy_grunt.mesh', icon: 'cube' },
        { name: 'sky_dome.mesh', icon: 'sphere' },
        { name: 'terrain_patch.mesh', icon: 'terrain' },
    ];

    const lighting = ['realtime', 'mixed', 'baked'];

    // Monospaced labels set telemetry keys apart from prose.
    const telemetry = [
        { key: 'seq', value: '48213' },
        { key: 'delta', value: '0.0021 s' },
        { key: 'buttons', value: '0x0004' },
        { key: 'axis_l', value: '+0.512 / -0.087' },
    ];

    const description =
        'An inspector row is a two-column grid: a label column and the controls that take the width it leaves. ' +
        'A group header folds its rows away, and a numeric field scrubs under a drag.';

    // The property panel that the section edits. A color is its red, green, blue and alpha.
    let inspector = $state({
        name: 'Player',
        meshRef: 'player_base.mesh',
        position: [1.234, 0.0, -4.5],
        rotation: [0.0, 90.0, 0.0],
        scale: [1.0, 1.0, 1.0],
        mass: 80.0,
        drag: 0.08,
        lighting: 0,
        tint: [0.77, 0.38, 0.24, 1.0],
        emissive: [0.24, 0.38, 0.77, 1.0],
        meshIcon: 'cube',
        openTransform: true,
        openMesh: true,
        openRigidbody: false,
        castShadows: true,
        receiveShadows: true,
        useGravity: true,
    });

    const references = $derived([
        { name: 'player_base.mesh', icon: 'cube' },
        { name: 'player_pbr', tint: inspector.tint },
        { name: 'footsteps_grass_01.ogg', icon: 'audio' },
        { name: 'enemy_grunt.mesh', icon: 'alert', missing: true },
        { name: 'None', icon: 'plus', empty: true },
    ]);

    // Takes the asset of a chip that was dropped on the mesh field.
    function meshDropped(event) {
        if (event.value < assetChips.length) {
            inspector.meshRef = assetChips[event.value].name;
            inspector.meshIcon = assetChips[event.value].icon;
        }
    }
</script>

<SectionFrame number="11" title="Inspector Rows" lede="The workhorse of the property editor." {description}>
  <DemoPanel title="ENTITY PROPERTIES" tags="ENTITY . PLAYER" bare>
    <panel rules="bottom" direction="row" align="center" gap={space[3]} padding="{space[4]} {space[5]}" flex="fixed">
      <icon icon="cube" size={14} tone="secondary"></icon>
      <text-field value={inspector.name} size="sm" onchange={(event) => (inspector.name = event.text)}></text-field>
      <badge variant="accent">ENTITY</badge>
      <badge variant="neutral">0x7FF8_A2B0</badge>
    </panel>
    <inspector-group badge="CORE" badge-variant="accent" open={inspector.openTransform}
        onchange={(event) => (inspector.openTransform = event.value === 1)}>
      Transform
      <inspector-row>
        Position
        <vector-field value={inspector.position} step={0.05}
            onchange={(event) => (inspector.position = event.values)}></vector-field>
      </inspector-row>
      <inspector-row>
        Rotation
        <vector-field value={inspector.rotation} step={0.5} precision={1}
            onchange={(event) => (inspector.rotation = event.values)}></vector-field>
      </inspector-row>
      <inspector-row>
        Scale
        <vector-field value={inspector.scale} step={0.01} min={0}
            onchange={(event) => (inspector.scale = event.values)}></vector-field>
        <icon icon="link" size={11} tone="muted"></icon>
      </inspector-row>
    </inspector-group>
    <inspector-group open={inspector.openMesh} onchange={(event) => (inspector.openMesh = event.value === 1)}>
      Mesh Renderer
      <inspector-row>
        Mesh
        <asset-field icon={inspector.meshIcon} kind={dragAssetChip} ondrop={meshDropped}>
          {inspector.meshRef}
        </asset-field>
      </inspector-row>
      <inspector-row>
        Material
        <asset-field tint={inspector.tint}>player_pbr</asset-field>
      </inspector-row>
      <inspector-row>
        Tint
        <color-field value={inspector.tint} onchange={(event) => (inspector.tint = event.values)}></color-field>
      </inspector-row>
      <inspector-row>
        Emissive
        <color-field value={inspector.emissive}
            onchange={(event) => (inspector.emissive = event.values)}></color-field>
      </inspector-row>
      <inspector-row>
        Cast Shadows
        <checkbox value={inspector.castShadows}
            onchange={(event) => (inspector.castShadows = event.value === 1)}></checkbox>
      </inspector-row>
      <inspector-row>
        Receive Shadows
        <checkbox value={inspector.receiveShadows}
            onchange={(event) => (inspector.receiveShadows = event.value === 1)}></checkbox>
      </inspector-row>
      <inspector-row>
        Lighting
        <segmented size="sm">
          {#each lighting as label, i (label)}
            <segment selected={inspector.lighting === i} onclick={() => (inspector.lighting = i)}>{label}</segment>
          {/each}
        </segmented>
      </inspector-row>
    </inspector-group>
    <inspector-group open={inspector.openRigidbody}
        onchange={(event) => (inspector.openRigidbody = event.value === 1)}>
      Rigidbody
      <inspector-row>
        Mass
        <number-field value={inspector.mass} step={0.1} min={0} unit="kg" precision={1}
            onchange={(event) => (inspector.mass = event.value)}></number-field>
      </inspector-row>
      <inspector-row>
        Drag
        <slider value={inspector.drag} min={0} max={1} onchange={(event) => (inspector.drag = event.value)}></slider>
        <number-field value={inspector.drag} step={0.005} min={0} max={1} width={72} flex="fixed"
            onchange={(event) => (inspector.drag = event.value)}></number-field>
      </inspector-row>
      <inspector-row>
        Use Gravity
        <toggle value={inspector.useGravity}
            onchange={(event) => (inspector.useGravity = event.value === 1)}></toggle>
      </inspector-row>
    </inspector-group>
    <panel rules="top" direction="row" padding={space[4]} flex="fixed">
      <button icon="plus" size="sm" flex="fill">Add Component</button>
    </panel>
  </DemoPanel>
  <DemoPanel title="TELEMETRY ROWS" tags="MONO LABELS" bare>
    <panel-header meta="POLL 125 HZ">INPUT DEVICE</panel-header>
    {#each telemetry as row (row.key)}
      <inspector-row mono>{row.key}<label {...text.mono}>{row.value}</label></inspector-row>
    {/each}
  </DemoPanel>
  <DemoPanel title="ASSET REFERENCE" tags="PICK . LOCATE . CLEAR">
    <box gap={space[2]} max-width={460}>
      {#each references as field (field.name)}
        <asset-field icon={field.icon} tint={field.tint} missing={field.missing}
            empty={field.empty}>{field.name}</asset-field>
      {/each}
    </box>
  </DemoPanel>
  <DemoPanel title="ASSET DRAG" tags="DROP ON THE MESH SLOT">
    <!-- The gallery makes each chip the source of a payload that carries the chip's index, and shows a ghost of the
         chip in flight. The binding has no kind that makes a widget the source of a payload, so these chips do not
         drag, and nothing here starts the drop that the mesh field above takes. -->
    <box direction="row" align="center" wrap gap={space[2]}>
      {#each assetChips as chip (chip.name)}
        <button icon={chip.icon} size="sm">{chip.name}</button>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
