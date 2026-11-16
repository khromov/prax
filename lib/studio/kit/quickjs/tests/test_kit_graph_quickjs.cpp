// The kinds of Studio Kit's node graphs in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/kit_box.h"
#include "pr/kit_color.h"
#include "pr/kit_graph.h"
#include "pr/kit_text.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// The script's record of the events that it hears: each with its node, its type and its detail, in `heard`. `count` and
// `last` tell how many events of a type a node queued, and the detail of the last of them. A test's `answer`, if it
// defines one, hears each event too.
const char* const kHear = R"JS(
    var heard = [];
    kitTree.onevent = function (node, type, detail) {
        heard.push({node: node, type: type, detail: detail});
        if (typeof answer === 'function')
            answer(node, type, detail);
    };
    function count(node, type) {
        return heard.filter(function (event) { return event.node === node && event.type === type; }).length;
    }
    function last(node, type) {
        for (var i = heard.length - 1; i >= 0; --i) {
            if (heard[i].node === node && heard[i].type === type)
                return heard[i].detail;
        }
        return null;
    }
)JS";

// The script that makes the graph of DirectGraph, and listens for every event of its nodes but those that tell
// geometry.
const char* const kGraphScript = R"JS(
    var graph = make('graph', 0, {width: '600', height: '400', flex: 'fixed', 'type-colors': '#6699e6 axis-y'});
    var sampler = make('graph-node', graph, {id: '1', position: '20 20', meta: 'texture'}, 'Sampler');
    var samplerColor = make('graph-port', sampler, {id: '1', side: 'output', type: '1'}, 'Color');
    var samplerAlpha = make('graph-port', sampler, {id: '2', side: 'output'}, 'Alpha');
    var blend = make('graph-node', graph, {id: '2', position: '260 20'}, 'Blend');
    var blendBase = make('graph-port', blend, {id: '1', side: 'input', type: '1'}, 'Base');
    var blendOut = make('graph-port', blend, {id: '3', side: 'output', type: '1'}, 'Out');
    var blendMask = make('graph-port', blend, {id: '2'}, 'Mask');
    var output = make('graph-node', graph, {id: '3', position: '260 200', 'body-height': '40'}, 'Output');
    var outputColor = make('graph-port', output, {id: '1', type: '1'}, 'Color');
    var body = make('graph-body', output, {});
    make('label', body, {}, 'Preview');
    var wire = make('graph-wire', graph, {id: '7', 'from-node': '1', 'from-port': '1', 'to-node': '2', 'to-port': '1'});
    ['connect', 'remove', 'view'].forEach(function (type) { kitTree.listen(graph, type, true); });
    [sampler, blend, output].forEach(function (node) {
        ['change', 'select', 'remove'].forEach(function (type) { kitTree.listen(node, type, true); });
    });
    ['select', 'remove'].forEach(function (type) { kitTree.listen(wire, type, true); });
)JS";

// A graph of three nodes on a canvas 600 by 400 px that the kit's own calls declare: a sampler, whose color feeds a
// blend's base, and an output with a body under its port, which holds a label. A wire that the user asks for joins the
// model, and what is selected leaves it when the user asks to remove it.
struct DirectGraph {
    KitFixture fixture;
    PrKitGraphPort samplerOut[2] = {{.id = 1, .type = 1, .label = "Color"}, {.id = 2, .type = 0, .label = "Alpha"}};
    PrKitGraphPort blendIn[2] = {{.id = 1, .type = 1, .label = "Base"}, {.id = 2, .type = 0, .label = "Mask"}};
    PrKitGraphPort blendOut[1] = {{.id = 3, .type = 1, .label = "Out"}};
    PrKitGraphPort outputIn[1] = {{.id = 1, .type = 1, .label = "Color"}};
    PrKitGraphNode nodes[3] = {
        {.id = 1, .position = {20, 20}, .title = "Sampler", .meta = "texture", .outputs = samplerOut, .outputCount = 2},
        {
            .id = 2,
            .position = {260, 20},
            .title = "Blend",
            .inputs = blendIn,
            .inputCount = 2,
            .outputs = blendOut,
            .outputCount = 1,
        },
        {.id = 3, .position = {260, 200}, .title = "Output", .inputs = outputIn, .inputCount = 1, .bodyHeight = 40},
    };
    Array<PrKitGraphWire> wires;
    PrColor colors[2] = {};
    uint32_t typeCount = 2;
    PrKitGraphView view{.zoom = 1};
    bool boxAfter = false;          // an empty box follows the graph
    bool disabled = false;          // the graph is under a disabled box
    PrLayoutNode canvas = kPrLayoutNodeNone;
    Array<PrKitGraphLink> connected;
    uint32_t removals = 0;

    DirectGraph() {
        prKitColorParseHex("#6699e6", &colors[0]);
        colors[1] = fixture.kit->theme.palette.axisY;
        wires.push(PrKitGraphWire{.id = 7, .link = {.fromNode = 1, .fromPort = 1, .toNode = 2, .toPort = 1}});
        fixture.build = [this](PrLayoutNode root) {
            PrKitGraph graph{
                .nodes = nodes,
                .nodeCount = 3,
                .wires = wires.data(),
                .wireCount = wires.count(),
                .typeColors = colors,
                .typeCount = typeCount,
                .view = &view,
            };
            PrLayoutNode parent = disabled ? prKitBox(fixture.kit, root, {.disabled = true}, {}) : root;
            canvas = prKitGraph(fixture.kit, parent, graph,
                {.place = {.width = prKitPx(600), .height = prKitPx(400), .flex = PrKitFlex_Fixed}});
            if (boxAfter)
                prKitBox(fixture.kit, root, {}, {});
            PrLayoutNode body = prKitGraphBody(fixture.kit, canvas, 2);
            if (body != kPrLayoutNodeNone)
                prKitLabel(fixture.kit, body, {.text = "Preview"}, {});
            PrKitGraphLink link{};
            if (prKitConnected(fixture.kit, canvas, &link)) {
                connected.push(link);
                wires.push(PrKitGraphWire{.id = 8, .link = link});
            }
            if (prKitRemoved(fixture.kit, canvas)) {
                ++removals;
                Array<PrKitGraphWire> kept;
                for (const PrKitGraphWire& wire : wires) {
                    if (!wire.selected)
                        kept.push(wire);
                }
                wires = kept;
            }
        };
    }

    // Returns where a point of the graph is on the screen.
    PrVec2 at(PrVec2 point) {
        PrRect box = fixture.bounds(canvas);
        return PrVec2{box.left + view.offset.x + point.x * view.zoom, box.top + view.offset.y + point.y * view.zoom};
    }
    PrVec2 port(uint32_t node, PrKitGraphSide side, uint32_t index) {
        return at(prKitGraphPortAnchor(&nodes[node], side, index));
    }
};

// A script's graph and the same graph of the kit's own calls.
struct GraphPair {
    JsKitScripted scripted;
    DirectGraph direct;

    GraphPair() {
        scripted.run(kJsKitMake);
        scripted.run(kHear);
        scripted.run(kGraphScript);
    }

    void frame() {
        scripted.fixture.frame();
        direct.fixture.frame();
    }
    void settle() {
        for (uint32_t i = 0; i < 30; ++i)
            frame();
    }

    // Presses the primary button at `from` in both, moves to `to`, and releases there.
    void drag(PrVec2 from, PrVec2 to, PrAppModifierFlags modifiers = 0) {
        KitFixture* both[] = {&scripted.fixture, &direct.fixture};
        for (KitFixture* f : both) {
            f->push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .modifiers = modifiers, .pointer = {.position = from}});
            f->frame();
            f->push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .modifiers = modifiers, .pointer = {.position = to}});
            f->frame();
            f->push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .modifiers = modifiers, .pointer = {.position = to}});
            f->frame();
        }
    }
    void click(PrVec2 at, PrAppModifierFlags modifiers = 0) {
        scripted.fixture.click(at, modifiers);
        direct.fixture.click(at, modifiers);
        frame();
    }
    void key(PrAppKey key) {
        scripted.fixture.key(key);
        direct.fixture.key(key);
        frame();
    }
    void push(PrAppEvent event) {
        scripted.fixture.push(event);
        direct.fixture.push(event);
        frame();
    }

    // Returns true if both render the same pixels, and the script's tree is laid out as a layout from scratch lays it
    // out.
    bool alike() {
        return jsKitRendersAlike(scripted.fixture, direct.fixture) && kitVerified(scripted.fixture.tree);
    }
};

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a graph that a script describes renders as the kit's graph of the same model does") {
    GraphPair pair;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.fixture.paintedCommands());
    CHECK(pair.scripted.number("heard.length") == 0);

    // The view, a node's width, and what is selected, as the script sets them.
    pair.scripted.run(R"JS(
        kitTree.set(graph, 'view', '30 -12 1.5');
        kitTree.set(sampler, 'width', '150');
        kitTree.set(blend, 'selected', '');
        kitTree.set(wire, 'selected', 'true');
        kitTree.set(output, 'body-height', '64');
        kitTree.set(sampler, 'meta', null);
        kitTree.set(graph, 'type-colors', 'warning #33cc8080');
    )JS");
    DirectGraph& direct = pair.direct;
    direct.view = PrKitGraphView{.offset = {30, -12}, .zoom = 1.5f};
    direct.nodes[0].width = 150;
    direct.nodes[1].selected = true;
    direct.wires[0].selected = true;
    direct.nodes[2].bodyHeight = 64;
    direct.nodes[0].meta = PrStringView{};
    direct.colors[0] = direct.fixture.kit->theme.palette.warning;
    prKitColorParseHex("#33cc8080", &direct.colors[1]);
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard.length") == 0);

    // Without them, each is its default: no body, the default width, and the accent for every port.
    pair.scripted.run(R"JS(
        ['view', 'type-colors'].forEach(function (name) { kitTree.set(graph, name, null); });
        kitTree.set(sampler, 'width', null);
        kitTree.set(blend, 'selected', null);
        kitTree.set(wire, 'selected', 'false');
        kitTree.set(output, 'body-height', null);
    )JS");
    direct.view = PrKitGraphView{.zoom = 1};
    direct.nodes[0].width = 0;
    direct.nodes[1].selected = false;
    direct.wires[0].selected = false;
    direct.nodes[2].bodyHeight = 0;
    direct.typeCount = 0;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
}

TEST_CASE("a drag on a node of a graph tells the script where each selected node went, and what the press selected") {
    GraphPair pair;
    pair.frame();
    pair.frame();
    DirectGraph& direct = pair.direct;
    pair.click(direct.at({300, 210}));
    CHECK(direct.nodes[2].selected);
    CHECK(pair.scripted.number("count(output, 'select')") == 1);
    CHECK(pair.scripted.number("last(output, 'select').value") == 1);
    CHECK(pair.scripted.number("heard.length") == 1);

    // With Shift, the press on the sampler adds it to the selection, and the drag moves both nodes, and with the
    // sampler its ports.
    pair.scripted.run("kitTree.listen(samplerAlpha, 'anchor', true);");
    pair.drag(direct.at({60, 30}), direct.at({90, 70}), PrAppModifierFlag_Shift);
    PrVec2 alpha = prKitGraphPortAnchor(&direct.nodes[0], PrKitGraphSide_Output, 1);
    CHECK(pair.scripted.number("count(samplerAlpha, 'anchor')") >= 2);
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[0]") == double(alpha.x));
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[1]") == double(alpha.y));
    CHECK(direct.nodes[0].position.x == doctest::Approx(50.0));
    CHECK(direct.nodes[2].position.y == doctest::Approx(240.0));
    CHECK(pair.scripted.number("last(sampler, 'select').value") == 1);
    CHECK(pair.scripted.number("count(sampler, 'change')") >= 1);
    CHECK(pair.scripted.number("last(sampler, 'change').values.length") == 2);
    CHECK(pair.scripted.number("last(sampler, 'change').values[0]") == double(direct.nodes[0].position.x));
    CHECK(pair.scripted.number("last(sampler, 'change').values[1]") == double(direct.nodes[0].position.y));
    CHECK(pair.scripted.number("last(output, 'change').values[0]") == double(direct.nodes[2].position.x));
    CHECK(pair.scripted.number("last(output, 'change').values[1]") == double(direct.nodes[2].position.y));
    CHECK(pair.scripted.number("count(blend, 'change') + count(blend, 'select')") == 0);
    pair.settle();
    CHECK(pair.alike());

    // What the drag left stands until the script says otherwise.
    pair.scripted.run("kitTree.set(sampler, 'position', '20 20'); kitTree.set(output, 'selected', null);");
    direct.nodes[0].position = PrVec2{20, 20};
    direct.nodes[2].selected = false;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
}

TEST_CASE("a wire dropped on a port of a graph asks the script to connect the two, which the script does") {
    GraphPair pair;
    pair.scripted.run(R"JS(
        function answer(node, type, detail) {
            if (type !== 'connect')
                return;
            make('graph-wire', graph, {
                'from-node': String(detail.values[0]), 'from-port': String(detail.values[1]),
                'to-node': String(detail.values[2]), 'to-port': String(detail.values[3]),
            });
        }
    )JS");
    pair.frame();
    pair.frame();
    DirectGraph& direct = pair.direct;
    // The sampler's alpha onto the blend's color input: unlike types, which the graph refuses.
    pair.drag(direct.port(0, PrKitGraphSide_Output, 1), direct.port(1, PrKitGraphSide_Input, 0));
    CHECK(direct.connected.isEmpty());
    CHECK(pair.scripted.number("count(graph, 'connect')") == 0);
    // The blend's mask, dragged onto the sampler's alpha, is joined from the output to the input.
    pair.drag(direct.port(1, PrKitGraphSide_Input, 1), direct.port(0, PrKitGraphSide_Output, 1));
    REQUIRE(direct.connected.count() == 1);
    CHECK(pair.scripted.number("count(graph, 'connect')") == 1);
    CHECK(pair.scripted.number("last(graph, 'connect').values.join() === '1,2,2,2' ? 1 : 0") == 1);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a graph names a node and a port that have no id by their numbers in the tree") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kHear);
    scripted.run(R"JS(
        var graph = make('graph', 0, {width: '600', height: '400', flex: 'fixed'});
        var source = make('graph-node', graph, {position: '20 20'}, 'Source');
        var out = make('graph-port', source, {side: 'output'}, 'Out');
        var sink = make('graph-node', graph, {position: '260 20'}, 'Sink');
        var into = make('graph-port', sink, {}, 'In');
        kitTree.listen(graph, 'connect', true);
    )JS");
    KitFixture& f = scripted.fixture;
    f.frame();
    f.frame();
    PrKitGraphPort port{.id = 1};
    PrKitGraphNode source{.position = {20, 20}, .outputs = &port, .outputCount = 1};
    PrKitGraphNode sink{.position = {260, 20}, .inputs = &port, .inputCount = 1};
    PrVec2 from = prKitGraphPortAnchor(&source, PrKitGraphSide_Output, 0);
    PrVec2 to = prKitGraphPortAnchor(&sink, PrKitGraphSide_Input, 0);
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerDown, .pointer = {.position = from}});
    f.frame();
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerMove, .pointer = {.position = to}});
    f.frame();
    f.push(PrAppEvent{.kind = PrAppEventKind_PointerUp, .pointer = {.position = to}});
    f.frame();
    CHECK(scripted.number("count(graph, 'connect')") == 1);
    CHECK(scripted.number("last(graph, 'connect').values[0] - 4294967296 === source ? 1 : 0") == 1);
    CHECK(scripted.number("last(graph, 'connect').values[1] - 4294967296 === out ? 1 : 0") == 1);
    CHECK(scripted.number("last(graph, 'connect').values[2] - 4294967296 === sink ? 1 : 0") == 1);
    CHECK(scripted.number("last(graph, 'connect').values[3] - 4294967296 === into ? 1 : 0") == 1);
    CHECK(kitVerified(f.tree));
}

TEST_CASE("a click on a wire of a graph selects it, and Delete asks the script to remove what is selected") {
    GraphPair pair;
    pair.scripted.run(R"JS(
        function answer(node, type) {
            if (type === 'remove' && node !== graph)
                kitTree.destroy(node);
        }
    )JS");
    pair.frame();
    pair.frame();
    DirectGraph& direct = pair.direct;
    PrVec2 from = direct.port(0, PrKitGraphSide_Output, 0);
    PrVec2 to = direct.port(1, PrKitGraphSide_Input, 0);
    pair.click((from + to) / 2);
    CHECK(direct.wires[0].selected);
    CHECK(pair.scripted.number("count(wire, 'select')") == 1);
    CHECK(pair.scripted.number("last(wire, 'select').value") == 1);
    pair.settle();
    CHECK(pair.alike());

    pair.key(PrAppKey_Delete);
    CHECK(direct.removals == 1);
    CHECK(direct.wires.isEmpty());
    CHECK(pair.scripted.number("count(graph, 'remove')") == 1);
    CHECK(pair.scripted.number("count(wire, 'remove')") == 1);
    CHECK(pair.scripted.number("heard.filter(function (event) { return event.type === 'remove'; }).length") == 2);
    // The graph asks before the wire does.
    CHECK(pair.scripted.number("heard[heard.length - 2].node === graph ? 1 : 0") == 1);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a marquee over a graph tells the script each node that it selects, and a click on the canvas clears them") {
    GraphPair pair;
    pair.frame();
    pair.frame();
    DirectGraph& direct = pair.direct;
    // From empty canvas over the blend and the output.
    pair.drag(direct.at({240, 10}), direct.at({500, 210}));
    CHECK(direct.nodes[1].selected);
    CHECK(direct.nodes[2].selected);
    CHECK(pair.scripted.number("count(sampler, 'select')") == 0);
    CHECK(pair.scripted.number("last(blend, 'select').value") == 1);
    CHECK(pair.scripted.number("last(output, 'select').value") == 1);
    pair.settle();
    CHECK(pair.alike());

    // Delete asks to remove both, and the script keeps them.
    pair.key(PrAppKey_Delete);
    CHECK(direct.removals == 1);
    CHECK(pair.scripted.number("count(graph, 'remove')") == 1);
    CHECK(pair.scripted.number("count(blend, 'remove') + count(output, 'remove')") == 2);
    CHECK(pair.scripted.number("count(sampler, 'remove') + count(wire, 'remove')") == 0);

    pair.click(direct.at({560, 380}));
    CHECK_FALSE(direct.nodes[1].selected);
    CHECK(pair.scripted.number("count(blend, 'select')") == 2);
    CHECK(pair.scripted.number("last(blend, 'select').value") == 0);
    CHECK(pair.scripted.number("last(output, 'select').value") == 0);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("the wheel pans and zooms a graph, and tells the script the view, which stands until the script sets it") {
    GraphPair pair;
    pair.frame();
    pair.frame();
    DirectGraph& direct = pair.direct;
    PrVec2 over = direct.at({300, 300});
    pair.scripted.fixture.move(over);
    direct.fixture.move(over);
    pair.push(PrAppEvent{.kind = PrAppEventKind_Scroll, .scroll = {.position = over, .delta = PrVec2{20, 10}}});
    CHECK(direct.view.offset.x == -20.0f);
    CHECK(pair.scripted.number("count(graph, 'view')") == 1);
    CHECK(pair.scripted.number("last(graph, 'view').values.join() === '-20,-10,1' ? 1 : 0") == 1);
    pair.push(PrAppEvent{
        .kind = PrAppEventKind_Scroll,
        .modifiers = PrAppModifierFlag_Super,
        .scroll = {.position = over, .delta = PrVec2{0, -70}},
    });
    CHECK(direct.view.zoom > 2.0f);
    CHECK(pair.scripted.number("count(graph, 'view')") == 2);
    CHECK(pair.scripted.number("last(graph, 'view').values[0]") == double(direct.view.offset.x));
    CHECK(pair.scripted.number("last(graph, 'view').values[1]") == double(direct.view.offset.y));
    CHECK(pair.scripted.number("last(graph, 'view').values[2]") == double(direct.view.zoom));
    pair.settle();
    CHECK(pair.alike());

    // Setting the view that the event told changes nothing.
    pair.scripted.run("kitTree.set(graph, 'view', last(graph, 'view').values.join(' '));");
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    pair.scripted.run("kitTree.set(graph, 'view', '8 16 0.75');");
    direct.view = PrKitGraphView{.offset = {8, 16}, .zoom = 0.75f};
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    // A view of two numbers is at a zoom of 1.
    pair.scripted.run("kitTree.set(graph, 'view', '-40 24');");
    direct.view = PrKitGraphView{.offset = {-40, 24}, .zoom = 1};
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("count(graph, 'view')") == 2);
}

TEST_CASE("setting arrange arranges a graph's nodes once, and tells the script where each went") {
    GraphPair pair;
    pair.frame();
    pair.scripted.run("kitTree.set(graph, 'arrange', '');");
    DirectGraph& direct = pair.direct;
    prKitGraphArrange(direct.nodes, 3, direct.wires.data(), direct.wires.count());
    pair.frame();
    // The sampler feeds the blend, so the blend stands in the column after the one of the sampler and the output.
    CHECK(direct.nodes[0].position.x == 0.0f);
    CHECK(direct.nodes[1].position.x > 0.0f);
    CHECK(pair.scripted.number("count(sampler, 'change') + count(blend, 'change') + count(output, 'change')") == 3);
    const char* const xs[] = {
        "last(sampler, 'change').values[0]", "last(blend, 'change').values[0]", "last(output, 'change').values[0]",
    };
    const char* const ys[] = {
        "last(sampler, 'change').values[1]", "last(blend, 'change').values[1]", "last(output, 'change').values[1]",
    };
    for (uint32_t i = 0; i < 3; ++i) {
        CHECK(pair.scripted.number(xs[i]) == double(direct.nodes[i].position.x));
        CHECK(pair.scripted.number(ys[i]) == double(direct.nodes[i].position.y));
    }
    pair.frame();
    CHECK(pair.alike());

    // The arrangement is made once: a node that the script moves afterward stays, and a reset flag asks for nothing.
    pair.scripted.run("kitTree.set(blend, 'position', '300 150'); kitTree.set(graph, 'arrange', null);");
    direct.nodes[1].position = PrVec2{300, 150};
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("count(blend, 'change')") == 1);
}

TEST_CASE("a node of a graph tells its extent, and a port its anchor, to a script that listens") {
    GraphPair pair;
    pair.frame();
    pair.scripted.run(R"JS(
        kitTree.listen(output, 'resize', true);
        kitTree.listen(samplerAlpha, 'anchor', true);
        kitTree.listen(blendMask, 'anchor', true);
    )JS");
    pair.frame();
    DirectGraph& direct = pair.direct;
    PrRect box = prKitGraphNodeRect(&direct.nodes[2]);
    CHECK(pair.scripted.number("count(output, 'resize')") == 1);
    CHECK(pair.scripted.number("last(output, 'resize').values[0]") == double(box.right - box.left));
    CHECK(pair.scripted.number("last(output, 'resize').values[1]") == double(box.bottom - box.top));
    PrVec2 alpha = prKitGraphPortAnchor(&direct.nodes[0], PrKitGraphSide_Output, 1);
    PrVec2 mask = prKitGraphPortAnchor(&direct.nodes[1], PrKitGraphSide_Input, 1);
    CHECK(pair.scripted.number("count(samplerAlpha, 'anchor')") == 1);
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[0]") == double(alpha.x));
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[1]") == double(alpha.y));
    CHECK(pair.scripted.number("last(blendMask, 'anchor').values[0]") == double(mask.x));
    CHECK(pair.scripted.number("last(blendMask, 'anchor').values[1]") == double(mask.y));

    // Neither tells again until it changes: the node as its body grows, and the port as its node moves.
    pair.frame();
    CHECK(pair.scripted.number("heard.length") == 3);
    pair.scripted.run(R"JS(
        kitTree.set(output, 'body-height', '90');
        kitTree.set(output, 'width', '240');
        kitTree.set(sampler, 'position', '40 60');
    )JS");
    direct.nodes[2].bodyHeight = 90;
    direct.nodes[2].width = 240;
    direct.nodes[0].position = PrVec2{40, 60};
    pair.frame();
    box = prKitGraphNodeRect(&direct.nodes[2]);
    alpha = prKitGraphPortAnchor(&direct.nodes[0], PrKitGraphSide_Output, 1);
    CHECK(pair.scripted.number("count(output, 'resize')") == 2);
    CHECK(pair.scripted.number("last(output, 'resize').values[0]") == 240);
    CHECK(pair.scripted.number("last(output, 'resize').values[1]") == double(box.bottom - box.top));
    CHECK(pair.scripted.number("count(samplerAlpha, 'anchor')") == 2);
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[0]") == double(alpha.x));
    CHECK(pair.scripted.number("last(samplerAlpha, 'anchor').values[1]") == double(alpha.y));
    CHECK(pair.scripted.number("count(blendMask, 'anchor')") == 1);
    pair.frame();
    CHECK(pair.alike());

    // A script that stops listening and listens again hears where things are.
    pair.scripted.run("kitTree.listen(output, 'resize', false);");
    pair.frame();
    pair.scripted.run("kitTree.listen(output, 'resize', true);");
    pair.frame();
    CHECK(pair.scripted.number("count(output, 'resize')") == 3);
}

TEST_CASE("a graph of a script under a disabled box shows its model and edits none of it") {
    GraphPair pair;
    pair.scripted.run("var shell = make('box', 0, {disabled: ''}); kitTree.insert(shell, graph, 0);");
    DirectGraph& direct = pair.direct;
    direct.disabled = true;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    pair.drag(direct.at({60, 30}), direct.at({90, 70}));
    pair.drag(direct.port(1, PrKitGraphSide_Input, 1), direct.port(0, PrKitGraphSide_Output, 1));
    CHECK(direct.nodes[0].position.x == 20.0f);
    CHECK_FALSE(direct.nodes[0].selected);
    CHECK(direct.connected.isEmpty());
    CHECK(pair.scripted.number("heard.length") == 0);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a graph's kinds refuse values that do not suit them, and declare nothing outside a graph") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kGraphScript);
    scripted.run(R"JS(
        var refused = [
            kitTree.set(graph, 'view', '4'), kitTree.set(graph, 'view', '4 5 6 7'), kitTree.set(graph, 'view', 'x y'),
            kitTree.set(graph, 'view', '4 5 0'), kitTree.set(graph, 'view', '4 5 -2'), kitTree.set(graph, 'zoom', '2'),
            kitTree.set(graph, 'offset', '4 5'),
            kitTree.set(graph, 'type-colors', 'accent nocolor'), kitTree.set(graph, 'type-colors', '#12345'),
            kitTree.set(graph, 'arrange', 'now'), kitTree.set(graph, 'position', '1 2'),
            kitTree.set(sampler, 'id', '-1'), kitTree.set(sampler, 'id', '1.5'), kitTree.set(sampler, 'position', '7'),
            kitTree.set(sampler, 'body-height', '-4'), kitTree.set(sampler, 'selected', 'yes'),
            kitTree.set(sampler, 'side', 'input'), kitTree.set(samplerColor, 'side', 'left'),
            kitTree.set(samplerColor, 'type', 'color'), kitTree.set(samplerColor, 'id', ''),
            kitTree.set(wire, 'from-node', 'sampler'), kitTree.set(wire, 'to-port', '-1'),
            kitTree.set(wire, 'selected', '2'), kitTree.set(body, 'height-of-body', '4'),
            kitTree.listen(graph, 'change', true), kitTree.listen(sampler, 'connect', true),
            kitTree.listen(samplerColor, 'select', true), kitTree.listen(wire, 'change', true),
            kitTree.listen(body, 'remove', true),
        ].filter(Boolean).length;
        var loose = make('box', 0, {});
        ['graph-node', 'graph-port', 'graph-body', 'graph-wire'].forEach(function (kind) {
            make('label', make(kind, loose, {}, 'Loose'), {}, 'Under a part that no graph holds');
        });
        make('label', sampler, {}, 'A label that is no part of a node, which a graph does not declare');
        make('label', graph, {}, 'A label that is no part of a graph, which it does not declare');
    )JS");
    CHECK(scripted.number("refused") == 0);
    DirectGraph direct;
    direct.boxAfter = true;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    CHECK(kitVerified(scripted.fixture.tree));
}
