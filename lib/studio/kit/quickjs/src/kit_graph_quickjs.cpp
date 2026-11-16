// The kinds of Studio Kit's node graphs (pr/kit_graph.h): `graph`, `graph-node`, `graph-port`, `graph-body` and
// `graph-wire`.
//
// A `graph` is a canvas that shows the `graph-node` and `graph-wire` nodes under it, which are its model, and declares
// no other node under it. A graph under a disabled box shows its model and edits none of it. It shows no text.
//
//   view            two or three numbers separated by spaces. The first two are the x and y of the graph's origin on
//                   the canvas, in px from its top left corner. The third is the zoom, the canvas px for each of the
//                   graph's px, which is above zero, and 1 if it is not given. With no view, the origin is at the
//                   corner and the zoom is 1
//   type-colors     colors separated by spaces. The color at the index of a port's `type` is the color of the port and
//                   of the wires that leave it, and a port whose type is past the colors takes the accent. A color is
//                   one of the palette's by name, such as `accent` or `axis-x`, which follows the theme, or six or
//                   eight hex digits after an optional `#`
//   arrange         a flag. Setting it arranges the nodes once, in the next round: each node takes its place in columns
//                   from left to right, after the nodes that feed it by a wire (prKitGraphArrange), and queues `change`
//                   as if a drag had moved it. Resetting it does nothing
//
//   connect         the user dropped a wire from a port on a port of the other side and the same type. `values` are the
//                   link that the user asks for, by the ids of the script: the node and the port that the wire leaves,
//                   an output, and the node and the port that it enters, an input. The graph draws no wire until the
//                   script puts a `graph-wire` under it
//   remove          Delete or Backspace asked to remove the selection while the graph had focus. Each selected node and
//                   wire queues `remove` too, after the graph
//   view            the wheel panned or zoomed the canvas. `values` are the new value of `view`, all three numbers,
//                   which stand until the script sets `view`
//
// A `graph-node` is a box of the graph that it is under. Its text is its title. The `graph-port` nodes under it are its
// ports, the inputs in a column on its left and the outputs on its right, each side in the order of the nodes. The
// nodes under its first `graph-body` are its body's content. Of the placement that every kind takes, it reads `width`
// alone, in px, and is 180 px wide with none.
//
//   id              a whole number above zero that no other node of the graph has, by which wires and `connect` name
//                   the node. A node with none takes 2 to the power of 32 plus its number in the tree, which no wire
//                   can name
//   position        two numbers separated by a space: the x and y of its top left corner, in the graph's px
//   meta            a string, which is shown beside the title
//   body-height     a length: the height of its body in px, or zero for no body
//   selected        a flag
//
//   change          a drag moved the node, as it moves every selected node. `values` are its new x and y, which stand
//                   until the script sets `position`
//   select          the user selected or deselected the node. `value` is 1 or 0, which stands until the script sets
//                   `selected`
//   remove          its graph asked to remove the selection, which the node is in
//   resize          `values` are the width and the height of its box in the graph's px, which follow from its width,
//                   its ports and its body (prKitGraphNodeRect). It is queued when the script first listens, and
//                   whenever they change
//
// A `graph-port` is a port of the node that it is under. Its text is its label.
//
//   id              a whole number above zero that no other port on its side of the node has, by which wires and
//                   `connect` name the port. A port with none takes a number as a node does
//   side            `input` or `output`
//   type            a whole number. A wire joins two ports of one type alone
//
//   anchor          `values` are the x and y of the point where a wire joins the port, in the graph's px
//                   (prKitGraphPortAnchor). It is queued when the script first listens, and whenever the point moves,
//                   as it does while its node is dragged
//
// A `graph-body` holds the content of the body of the node that it is under. The nodes under it are declared in the
// body, a column that clips them, if the node has a body. It takes no attributes of its own and queues no events.
//
// A `graph-wire` is a curve from an output to an input, in the color of the output's type. A wire that names no port of
// its graph is not drawn. It shows no text.
//
//   from-node       the id of the node that it leaves
//   from-port       the id of that node's output that it leaves
//   to-node         the id of the node that it enters
//   to-port         the id of that node's input that it enters
//   selected        a flag
//   id              a whole number, which the kit keeps for the wire and nothing reads
//
//   select          the user selected the wire by a click on it, or deselected it. `value` is 1 or 0, which stands
//                   until the script sets `selected`
//   remove          its graph asked to remove the selection, which the wire is in
//
// A node, a port, a body or a wire that is not under the node that holds its kind declares nothing. The placement of a
// port, a body or a wire is not read.

#include "studio/kit/quickjs/src/kit_graph_quickjs.h"

#include "pr/color.h"
#include "pr/kit_color.h"
#include "pr/kit_graph.h"
#include "pr/raw_array.h"
#include "pr/raw_string.h"
#include "pr/vec.h"

namespace pr_impl {

namespace {

// The family's kinds, by their position among them.
enum {
    GraphKind_Graph,
    GraphKind_Node,
    GraphKind_Port,
    GraphKind_Body,
    GraphKind_Wire,
    GraphKind_Count,
};
typedef uint32_t GraphKind;
extern const JsKitKind kKinds[GraphKind_Count];

// The events of each kind, by their index.
enum { GraphEvent_Connect, GraphEvent_Remove, GraphEvent_View };
constexpr PrStringView kGraphEvents[] = {"connect", "remove", "view"};
enum { NodeEvent_Change, NodeEvent_Select, NodeEvent_Remove, NodeEvent_Resize };
constexpr PrStringView kNodeEvents[] = {"change", "select", "remove", "resize"};
enum { PortEvent_Anchor };
constexpr PrStringView kPortEvents[] = {"anchor"};
enum { WireEvent_Select, WireEvent_Remove };
constexpr PrStringView kWireEvents[] = {"select", "remove"};

constexpr JsKitKeyword kSides[] = {{"input", PrKitGraphSide_Input}, {"output", PrKitGraphSide_Output}};

// A color that an attribute gives: one of the palette's, which follows the kit's theme, or a value of its own if
// `palette` is JsKitColor_None.
struct GraphColor {
    JsKitColor palette;
    PrColor value;
};

struct GraphData {
    PrKitGraphView view;                // the zoom is 1 from the node's creation
    RawArray<GraphColor> typeColors;    // by port type
    bool arranging;                     // the script set `arrange`, which the next round answers

    // The model that a round gives the kit, which the round makes from the nodes under the graph. Each node's ports are
    // a range of `ports`, its inputs before its outputs. None holds anything from round to round but its storage.
    RawArray<PrKitGraphNode> nodes;
    RawArray<PrKitGraphPort> ports;
    RawArray<PrKitGraphWire> wires;
    RawArray<PrColor> colors;           // by port type

    // The script's node for each node, port and wire of the model, by its index there.
    RawArray<JsKitNodeIndex> nodeSources;
    RawArray<JsKitNodeIndex> portSources;
    RawArray<JsKitNodeIndex> wireSources;
};

struct NodeData {
    uint32_t id;                    // zero for none
    PrVec2 position;
    float bodyHeight;
    bool selected;
    PrRawString meta;

    // The extent that the node's last `resize` told, if `sized`. A node that the script does not listen to is not
    // sized, so that it tells its extent once the script listens.
    PrVec2 extent;
    bool sized;
};

struct PortData {
    uint32_t id;                    // zero for none
    uint32_t type;
    PrKitGraphSide side;

    // The point that the port's last `anchor` told, if `anchored`, which is kept as a node's extent is.
    PrVec2 anchor;
    bool anchored;
};

struct WireData {
    uint32_t id;                    // zero for none
    uint32_t fromNode;
    uint32_t fromPort;
    uint32_t toNode;
    uint32_t toPort;
    bool selected;
};

bool isKind(const JsKitNodeTree* tree, JsKitNodeIndex node, GraphKind kind) {
    return jsKitNodeKind(tree, node) == &kKinds[kind];
}

// Returns the id that the kit knows a node, a port or a wire by: the id that the script gave, or one made of the node's
// index, which is above every id that a script can give.
uint64_t idFrom(uint32_t given, JsKitNodeIndex node) {
    return given != 0 ? given : (uint64_t(1) << 32) + node;
}

// Finds the part of `text` at or after `*at`, where spaces separate the parts, and moves `*at` past it. Returns false
// if no part is left.
bool nextPart(PrStringView text, size_t* at, PrStringView* part) {
    size_t start = *at;
    while (start < text.size && text.data[start] == ' ')
        ++start;
    size_t end = start;
    while (end < text.size && text.data[end] != ' ')
        ++end;
    *at = end;
    *part = PrStringView{text.data + start, end - start};
    return end > start;
}

// Reads two numbers separated by spaces, or with `reset` writes zeros.
bool setPair(PrStringView value, bool reset, PrVec2* out) {
    float numbers[2] = {};
    size_t at = 0;
    PrStringView part = {};
    for (uint32_t i = 0; i < 2 && !reset; ++i) {
        if (!nextPart(value, &at, &part) || !jsKitSetFloat(part, false, &numbers[i]))
            return false;
    }
    if (!reset && nextPart(value, &at, &part))
        return false;
    *out = PrVec2{numbers[0], numbers[1]};
    return true;
}

// Sets a graph's view from the x and y of its offset and, if there is a third number, its zoom, or with `reset` to no
// offset at a zoom of 1. Returns false, leaving the view as it was, if `value` is anything else or the zoom is not
// above zero.
bool setView(PrStringView value, bool reset, PrKitGraphView* out) {
    float numbers[3] = {0, 0, 1};
    uint32_t count = 0;
    size_t at = 0;
    PrStringView part = {};
    while (!reset && nextPart(value, &at, &part)) {
        if (count == 3 || !jsKitSetFloat(part, false, &numbers[count]))
            return false;
        ++count;
    }
    if (!reset && (count < 2 || !(numbers[2] > 0)))
        return false;
    *out = PrKitGraphView{.offset = {numbers[0], numbers[1]}, .zoom = numbers[2]};
    return true;
}

// Reads a color: one of the palette's by name, or hex digits.
bool colorFrom(PrStringView text, GraphColor* out) {
    GraphColor color{};
    if (!jsKitKeywordFrom(jsKitColors(), text, &color.palette) && !prKitColorParseHex(text, &color.value))
        return false;
    *out = color;
    return true;
}

// Sets a graph's type colors from colors separated by spaces, or with `reset` to none. Returns false, leaving the
// colors as they were, if a part of `value` is not a color.
bool setColors(PrStringView value, bool reset, RawArray<GraphColor>* out) {
    size_t at = 0;
    PrStringView part = {};
    GraphColor color{};
    while (!reset && nextPart(value, &at, &part)) {
        if (!colorFrom(part, &color))
            return false;
    }
    out->clear();
    at = 0;
    while (!reset && nextPart(value, &at, &part)) {
        colorFrom(part, &color);
        out->push(color);
    }
    return true;
}

void graphCreate(void* data) {
    static_cast<GraphData*>(data)->view.zoom = 1;
}

void graphDestroy(void* data) {
    GraphData& graph = *static_cast<GraphData*>(data);
    graph.typeColors.reset();
    graph.nodes.reset();
    graph.ports.reset();
    graph.wires.reset();
    graph.colors.reset();
    graph.nodeSources.reset();
    graph.portSources.reset();
    graph.wireSources.reset();
}

bool graphSet(void* data, PrStringView name, PrStringView value, bool reset) {
    GraphData& graph = *static_cast<GraphData*>(data);
    if (name == "view")
        return setView(value, reset, &graph.view);
    if (name == "type-colors")
        return setColors(value, reset, &graph.typeColors);
    if (name == "arrange") {
        bool arrange = false;
        if (!jsKitSetFlag(value, reset, &arrange))
            return false;
        graph.arranging = graph.arranging || arrange;
        return true;
    }
    return false;
}

void nodeDestroy(void* data) {
    prRawStringReset(&static_cast<NodeData*>(data)->meta);
}

bool nodeSet(void* data, PrStringView name, PrStringView value, bool reset) {
    NodeData& node = *static_cast<NodeData*>(data);
    if (name == "id")
        return jsKitSetUint(value, reset, &node.id);
    if (name == "position")
        return setPair(value, reset, &node.position);
    if (name == "meta")
        return jsKitSetString(value, reset, &node.meta);
    if (name == "body-height")
        return jsKitSetLength(value, reset, &node.bodyHeight);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &node.selected);
    return false;
}

bool portSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PortData& port = *static_cast<PortData*>(data);
    if (name == "id")
        return jsKitSetUint(value, reset, &port.id);
    if (name == "side")
        return jsKitSetKeyword(kSides, value, reset, &port.side);
    if (name == "type")
        return jsKitSetUint(value, reset, &port.type);
    return false;
}

bool wireSet(void* data, PrStringView name, PrStringView value, bool reset) {
    WireData& wire = *static_cast<WireData*>(data);
    if (name == "id")
        return jsKitSetUint(value, reset, &wire.id);
    if (name == "from-node")
        return jsKitSetUint(value, reset, &wire.fromNode);
    if (name == "from-port")
        return jsKitSetUint(value, reset, &wire.fromPort);
    if (name == "to-node")
        return jsKitSetUint(value, reset, &wire.toNode);
    if (name == "to-port")
        return jsKitSetUint(value, reset, &wire.toPort);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &wire.selected);
    return false;
}

// A node, a port, a body or a wire declares nothing by itself: its graph reads it, and declares the nodes under a body
// in the body that the kit made.
PrLayoutNode partDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

// Makes the round's model from the nodes under the graph `node`.
void graphCollect(const JsKitNodeTree* tree, JsKitNodeIndex node, const PrKit& kit, GraphData& graph) {
    graph.nodes.clear();
    graph.ports.clear();
    graph.wires.clear();
    graph.colors.clear();
    graph.nodeSources.clear();
    graph.portSources.clear();
    graph.wireSources.clear();
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isKind(tree, child, GraphKind_Wire)) {
            const WireData& from = *static_cast<const WireData*>(jsKitNodeData(tree, child));
            graph.wires.push(PrKitGraphWire{
                .id = idFrom(from.id, child),
                .link = {from.fromNode, from.fromPort, from.toNode, from.toPort},
                .selected = from.selected,
            });
            graph.wireSources.push(child);
        }
        if (!isKind(tree, child, GraphKind_Node))
            continue;
        const NodeData& from = *static_cast<const NodeData*>(jsKitNodeData(tree, child));
        PrKitSize width = jsKitNodePlace(tree, child).width;
        PrKitGraphNode made{
            .id = idFrom(from.id, child),
            .position = from.position,
            .width = width.kind == PrKitSizeKind_Px ? width.value : 0,
            .title = jsKitNodeText(tree, child),
            .meta = prRawStringView(&from.meta),
            .bodyHeight = from.bodyHeight,
            .selected = from.selected,
        };
        for (PrKitGraphSide side = PrKitGraphSide_Input; side <= PrKitGraphSide_Output; ++side) {
            uint32_t count = 0;
            for (JsKitNodeIndex part = jsKitNodeFirstChild(tree, child); part != kJsKitNodeNone;
                    part = jsKitNodeNextSibling(tree, part)) {
                if (!isKind(tree, part, GraphKind_Port))
                    continue;
                const PortData& port = *static_cast<const PortData*>(jsKitNodeData(tree, part));
                if (port.side != side)
                    continue;
                graph.ports.push(PrKitGraphPort{
                    .id = idFrom(port.id, part),
                    .type = port.type,
                    .label = jsKitNodeText(tree, part),
                });
                graph.portSources.push(part);
                ++count;
            }
            (side == PrKitGraphSide_Input ? made.inputCount : made.outputCount) = count;
        }
        graph.nodes.push(made);
        graph.nodeSources.push(child);
    }
    // The ports are in their final storage only now, so each node takes its ranges of them here.
    uint32_t at = 0;
    for (uint32_t i = 0; i < graph.nodes.count; ++i) {
        PrKitGraphNode& made = graph.nodes.data[i];
        made.inputs = made.inputCount > 0 ? graph.ports.data + at : nullptr;
        at += made.inputCount;
        made.outputs = made.outputCount > 0 ? graph.ports.data + at : nullptr;
        at += made.outputCount;
    }
    for (uint32_t i = 0; i < graph.typeColors.count; ++i) {
        const GraphColor& color = graph.typeColors.data[i];
        graph.colors.push(color.palette != JsKitColor_None ? jsKitColorFrom(kit, color.palette) : color.value);
    }
}

// Queues an event of `node` that tells two numbers.
void pushPair(const JsKitRound& round, JsKitNodeIndex node, uint32_t event, PrVec2 pair) {
    JsKitDetail detail{.values = {double(pair.x), double(pair.y)}, .valueCount = 2};
    jsKitEventPush(round, node, event, &detail);
}

// Queues `select` of `node`, which tells whether the node is selected now.
void pushSelect(const JsKitRound& round, JsKitNodeIndex node, uint32_t event, bool selected) {
    JsKitDetail detail{.values = {selected ? 1.0 : 0.0}, .valueCount = 1};
    jsKitEventPush(round, node, event, &detail);
}

// Takes what the kit wrote to the round's model into the nodes that the model was made from, and queues their events.
// `removed` says that the kit asked to remove the selection.
void graphReport(const JsKitRound& round, GraphData& graph, bool removed) {
    const JsKitNodeTree* tree = round.tree;
    uint32_t port = 0;
    for (uint32_t i = 0; i < graph.nodes.count; ++i) {
        const PrKitGraphNode& made = graph.nodes.data[i];
        JsKitNodeIndex source = graph.nodeSources.data[i];
        NodeData& node = *static_cast<NodeData*>(jsKitNodeData(tree, source));
        if (made.position.x != node.position.x || made.position.y != node.position.y) {
            node.position = made.position;
            pushPair(round, source, NodeEvent_Change, made.position);
        }
        if (made.selected != node.selected) {
            node.selected = made.selected;
            pushSelect(round, source, NodeEvent_Select, made.selected);
        }
        if (removed && made.selected)
            jsKitEventPush(round, source, NodeEvent_Remove, nullptr);

        PrRect box = prKitGraphNodeRect(&made);
        PrVec2 extent{box.right - box.left, box.bottom - box.top};
        bool listens = jsKitNodeListens(tree, source, NodeEvent_Resize);
        if (listens && (!node.sized || extent.x != node.extent.x || extent.y != node.extent.y)) {
            node.extent = extent;
            pushPair(round, source, NodeEvent_Resize, extent);
        }
        node.sized = listens;

        for (PrKitGraphSide side = PrKitGraphSide_Input; side <= PrKitGraphSide_Output; ++side) {
            uint32_t count = side == PrKitGraphSide_Input ? made.inputCount : made.outputCount;
            for (uint32_t j = 0; j < count; ++j, ++port) {
                JsKitNodeIndex portSource = graph.portSources.data[port];
                PortData& data = *static_cast<PortData*>(jsKitNodeData(tree, portSource));
                PrVec2 anchor = prKitGraphPortAnchor(&made, side, j);
                bool heard = jsKitNodeListens(tree, portSource, PortEvent_Anchor);
                if (heard && (!data.anchored || anchor.x != data.anchor.x || anchor.y != data.anchor.y)) {
                    data.anchor = anchor;
                    pushPair(round, portSource, PortEvent_Anchor, anchor);
                }
                data.anchored = heard;
            }
        }
    }
    for (uint32_t i = 0; i < graph.wires.count; ++i) {
        const PrKitGraphWire& made = graph.wires.data[i];
        JsKitNodeIndex source = graph.wireSources.data[i];
        WireData& wire = *static_cast<WireData*>(jsKitNodeData(tree, source));
        if (made.selected != wire.selected) {
            wire.selected = made.selected;
            pushSelect(round, source, WireEvent_Select, made.selected);
        }
        if (removed && made.selected)
            jsKitEventPush(round, source, WireEvent_Remove, nullptr);
    }
}

PrLayoutNode graphDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    GraphData& graph = jsKitData<GraphData>(round, node);
    const JsKitNodeTree* tree = round.tree;
    PrKit* kit = round.kit;
    graphCollect(tree, node, *kit, graph);
    if (graph.arranging) {
        // The arrangement moves the model's nodes as a drag does, so the report below tells each node its place.
        prKitGraphArrange(graph.nodes.data, graph.nodes.count, graph.wires.data, graph.wires.count);
        graph.arranging = false;
    }
    PrKitGraphView view = graph.view;
    PrKitGraph content{
        .key = jsKitNodeKey(round, node),
        .nodes = graph.nodes.data,
        .nodeCount = graph.nodes.count,
        .wires = graph.wires.data,
        .wireCount = graph.wires.count,
        .typeColors = graph.colors.data,
        .typeCount = graph.colors.count,
        .view = &graph.view,
    };
    PrLayoutNode canvas = prKitGraph(kit, parent, content, {.place = jsKitNodePlace(tree, node)});
    for (uint32_t i = 0; i < graph.nodes.count; ++i) {
        PrLayoutNode body = prKitGraphBody(kit, canvas, i);
        JsKitNodeIndex part = jsKitNodeFirstChild(tree, graph.nodeSources.data[i]);
        while (part != kJsKitNodeNone && !isKind(tree, part, GraphKind_Body))
            part = jsKitNodeNextSibling(tree, part);
        if (body != kPrLayoutNodeNone && part != kJsKitNodeNone)
            jsKitDeclareChildren(round, part, body);
    }

    PrKitGraphLink link{};
    if (prKitConnected(kit, canvas, &link)) {
        JsKitDetail detail{
            .values = {double(link.fromNode), double(link.fromPort), double(link.toNode), double(link.toPort)},
            .valueCount = 4,
        };
        jsKitEventPush(round, node, GraphEvent_Connect, &detail);
    }
    bool removed = prKitRemoved(kit, canvas);
    if (removed)
        jsKitEventPush(round, node, GraphEvent_Remove, nullptr);
    graphReport(round, graph, removed);
    if (graph.view.offset.x != view.offset.x || graph.view.offset.y != view.offset.y || graph.view.zoom != view.zoom) {
        JsKitDetail detail{
            .values = {double(graph.view.offset.x), double(graph.view.offset.y), double(graph.view.zoom)},
            .valueCount = 3,
        };
        jsKitEventPush(round, node, GraphEvent_View, &detail);
    }
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[GraphKind_Count] = {
    {
        .name = "graph",
        .dataSize = sizeof(GraphData),
        .events = kGraphEvents,
        .eventCount = 3,
        .flags = JsKitKindFlag_PlacesItself,
        .create = graphCreate,
        .destroy = graphDestroy,
        .set = graphSet,
        .declare = graphDeclare,
    },
    {
        .name = "graph-node",
        .dataSize = sizeof(NodeData),
        .events = kNodeEvents,
        .eventCount = 4,
        .destroy = nodeDestroy,
        .set = nodeSet,
        .declare = partDeclare,
    },
    {
        .name = "graph-port",
        .dataSize = sizeof(PortData),
        .events = kPortEvents,
        .eventCount = 1,
        .set = portSet,
        .declare = partDeclare,
    },
    {.name = "graph-body", .declare = partDeclare},
    {
        .name = "graph-wire",
        .dataSize = sizeof(WireData),
        .events = kWireEvents,
        .eventCount = 2,
        .set = wireSet,
        .declare = partDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitGraphKinds() {
    return kKinds;
}

} // namespace pr_impl
