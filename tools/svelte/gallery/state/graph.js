// The graph section's model (examples/studio_gallery/gallery_state.cpp): the types of its ports, and the nodes and
// wires of its material.

// The types of the graph section's ports, which index its colors.
export const PortType = { scalar: 0, vector: 1, color: 2 };

// The ports of the graph's nodes.
const samplerOut = [
    { id: 1, type: PortType.color, label: 'rgb' },
    { id: 2, type: PortType.scalar, label: 'alpha' },
];
const noiseOut = [{ id: 1, type: PortType.scalar, label: 'value' }];
const blendIn = [
    { id: 1, type: PortType.color, label: 'a' },
    { id: 2, type: PortType.color, label: 'b' },
    { id: 3, type: PortType.scalar, label: 'mix' },
];
const blendOut = [{ id: 1, type: PortType.color, label: 'out' }];
const surfaceIn = [
    { id: 1, type: PortType.color, label: 'color' },
    { id: 2, type: PortType.scalar, label: 'opacity' },
    { id: 3, type: PortType.vector, label: 'normal' },
];

// Returns the graph's material: its nodes, its wires, and the id of the next wire that the user makes.
export function seedGraph() {
    let nextWireId = 1;
    const node = (id, position, title, meta, inputs, outputs, bodyHeight = 0) => (
        { id, position, title, meta, inputs, outputs, bodyHeight, selected: false }
    );
    const wire = (fromNode, fromPort, toNode, toPort) => (
        { id: nextWireId++, fromNode, fromPort, toNode, toPort, selected: false }
    );
    return {
        nodes: [
            node(1, [24, 36], 'Sampler', 'texture2d', [], samplerOut),
            node(2, [40, 190], 'Noise', 'generator', [], noiseOut),
            node(3, [270, 96], 'Blend', 'mix', blendIn, blendOut, 28),
            node(4, [510, 130], 'Surface', 'output', surfaceIn, []),
        ],
        wires: [wire(1, 1, 3, 1), wire(2, 1, 3, 3), wire(3, 1, 4, 1)],
        nextWireId,
    };
}
