<!-- The gallery's sectionGraph (examples/studio_gallery/section_data.cpp). -->
<script>
    import { tick } from 'svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { PortType, seedGraph } from '../state/graph.js';
    import { height, space, text } from '../tokens.js';

    // The color of each type of port, by its channels.
    const typeColors = [
        [0.41, 0.62, 0.87], // scalar
        [0.46, 0.76, 0.54], // vector
        [0.89, 0.64, 0.35], // color
    ];
    const names = ['scalar', 'vector', 'color'];

    // The blend node's body shows its mix from its first input's color to its second's. A cell's channel is the
    // single-precision number that the example computes, whose build rounds the product and the sum once, together.
    const cells = 12;
    const mix = Array.from({ length: cells }, (_, cell) => {
        const t = Math.fround(cell / (cells - 1));
        const from = typeColors[PortType.color].map(Math.fround);
        const to = typeColors[PortType.vector].map(Math.fround);
        return from.map((channel, index) => Math.fround(channel + Math.fround(to[index] - channel) * t)).join(' ');
    });

    const description =
        "The kit's graph draws the canvas, the node boxes and the wires, and hands back each node's body, where " +
        "the blend node's swatch is declared with the same kit calls as the rest of the page. Drag a node to " +
        'move it and every node selected with it; drag from a port to a port of the same type to wire them, and ' +
        'an unlike pair refuses. Drag on empty canvas to select, click a wire to pick it, and Delete removes ' +
        'what is selected. The wheel pans, and Cmd with the wheel zooms.';

    // The graph section's material: its nodes and wires, and the view.
    const seed = seedGraph();
    let nodes = $state(seed.nodes);
    let wires = $state(seed.wires);
    let view = $state.raw([16, 8, 1]);
    let nextWireId = seed.nextWireId;

    // True from the Arrange button's click until the graph has been told to arrange its nodes.
    let arranging = $state(false);

    const selected = $derived(nodes.filter((node) => node.selected).length);
    const picked = $derived(wires.filter((wire) => wire.selected).length);

    // The graph arranges its nodes when `arrange` is set, and tells each node its place. The attribute is reset once
    // it has been set, so that the next click sets it again.
    async function arrange() {
        arranging = true;
        await tick();
        arranging = false;
    }

    // A wire into an input replaces the wire that fed it.
    function connect(event) {
        const [fromNode, fromPort, toNode, toPort] = event.values;
        wires = wires.filter((wire) => wire.toNode !== toNode || wire.toPort !== toPort);
        wires.push({ id: nextWireId++, fromNode, fromPort, toNode, toPort, selected: false });
    }

    // Removes the selected nodes, the selected wires, and the wires of the nodes that left.
    function remove() {
        nodes = nodes.filter((node) => !node.selected);
        const held = (id) => nodes.some((node) => node.id === id);
        wires = wires.filter((wire) => !wire.selected && held(wire.fromNode) && held(wire.toNode));
    }
</script>

<!-- A node's ports of a side, `input` or `output`, in their order. -->
{#snippet ports(node, side)}
  {#each side === 'input' ? node.inputs : node.outputs as port (port.id)}
    <graph-port id={port.id} {side} type={port.type}>{port.label}</graph-port>
  {/each}
{/snippet}

<SectionFrame number="22" title="Node Graph" lede="Typed nodes joined by wires." {description}>
  <panel rules="bottom" direction="row" align="center" gap={space[3]} padding="0 {space[4]}" height={height.md}
      flex="fixed">
    <button size="sm" onclick={arrange}>Arrange</button>
    <button size="sm" disabled={selected + picked === 0} onclick={remove}>Delete</button>
    <spacer></spacer>
    <label {...text.monoMuted}>{nodes.length} nodes . {wires.length} wires . {selected + picked} selected</label>
  </panel>

  <DemoPanel title="MATERIAL GRAPH" tags="EDITABLE" bare>
    <graph {view} type-colors={typeColors.map((color) => color.join(','))} arrange={arranging} height={340}
        flex="fixed" onconnect={connect} onremove={remove} onview={(event) => (view = event.values)}>
      {#each nodes as node (node.id)}
        <graph-node id={node.id} position={node.position} meta={node.meta} body-height={node.bodyHeight}
            selected={node.selected} onchange={(event) => (node.position = event.values)}
            onselect={(event) => (node.selected = event.value === 1)}>
          {node.title}
          {@render ports(node, 'input')}
          {@render ports(node, 'output')}
          {#if node.bodyHeight > 0}
            <graph-body>
              <box direction="row" gap={0}>
                {#each mix as color}
                  <swatch {color} size={12}></swatch>
                {/each}
              </box>
            </graph-body>
          {/if}
        </graph-node>
      {/each}
      {#each wires as wire (wire.id)}
        <graph-wire id={wire.id} from-node={wire.fromNode} from-port={wire.fromPort} to-node={wire.toNode}
            to-port={wire.toPort} selected={wire.selected} onselect={(event) => (wire.selected = event.value === 1)}>
        </graph-wire>
      {/each}
    </graph>

    <box direction="row" align="center" gap={space[6]} padding={space[4]}>
      {#each names as name, type (name)}
        <box direction="row" gap={space[2]}>
          <swatch color={typeColors[type].join(' ')} size={8}></swatch>
          <label tone="secondary" wrap="nowrap">{name}</label>
        </box>
      {/each}
    </box>
  </DemoPanel>
</SectionFrame>
