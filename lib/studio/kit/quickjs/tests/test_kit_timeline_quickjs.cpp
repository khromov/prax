// The kinds of Studio Kit's timelines in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/kit_box.h"
#include "pr/kit_color.h"
#include "pr/kit_timeline.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// The px of the gutter and the ruler, and of a track, as the timeline draws them by default.
constexpr float kGutter = 96;
constexpr float kRuler = 20;
constexpr float kTrack = 26;

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

// The script that makes the timeline of DirectTimeline, and listens for every event of the timeline and its blocks.
const char* const kTimelineScript = R"JS(
    var timeline = make('timeline', 0, {width: '496', flex: 'fixed', view: '0 8', playhead: '0', 'snap-step': '0.25'});
    var position = make('timeline-track', timeline, {}, 'Position');
    var walk = make('timeline-block', position, {start: '1', duration: '2', color: '#4d80cc'}, 'Walk');
    var run = make('timeline-block', position, {start: '4', duration: '1', color: '#4d80cc'}, 'Run');
    var rotation = make('timeline-track', timeline, {}, 'Rotation');
    var cue = make('timeline-marker', rotation, {time: '6.5', color: 'danger'});
    var turn = make('timeline-block', rotation, {start: '2', duration: '2', color: 'axis-x'}, 'Turn');
    var tracks = [position, rotation];
    ['change', 'view', 'add', 'remove'].forEach(function (type) { kitTree.listen(timeline, type, true); });
    [walk, run, turn].forEach(function (block) {
        ['change', 'select', 'remove'].forEach(function (type) { kitTree.listen(block, type, true); });
    });
)JS";

// A timeline of two tracks over eight seconds that the kit's own calls declare. Its lanes are 400 px wide, so that a
// second is 50 px: Walk from 1 s to 3 s and Run from 4 s to 5 s on the first track, and Turn from 2 s to 4 s and a
// marker at 6.5 s on the second. A block that the user asks for joins the model, and the selected blocks leave it when
// the user asks to remove them.
struct DirectTimeline {
    KitFixture fixture;
    PrKitTimelineTrack tracks[2] = {{"Position"}, {"Rotation"}};
    Array<PrKitTimelineBlock> blocks;
    Array<PrKitTimelineMarker> markers;
    PrKitTimeSpan view{0, 8};
    PrKitTimeSpan loop{};
    float playhead = 0;
    bool hasPlayhead = true;
    bool hasLoop = false;
    bool readOnly = false;
    PrKitTimelineSnap snap{.step = 0.25f};
    float trackHeight = 0;
    PrLayoutNode frame = kPrLayoutNodeNone;
    uint32_t removals = 0;
    uint32_t additions = 0;
    bool boxAfter = false;          // an empty box follows the timeline
    bool disabled = false;          // the timeline is under a disabled box

    DirectTimeline() {
        const PrKitPalette& palette = fixture.kit->theme.palette;
        PrColor blue{};
        prKitColorParseHex("#4d80cc", &blue);
        blocks.push(PrKitTimelineBlock{.id = 1, .track = 0, .start = 1, .duration = 2, .color = blue, .label = "Walk"});
        blocks.push(PrKitTimelineBlock{.id = 2, .track = 0, .start = 4, .duration = 1, .color = blue, .label = "Run"});
        PrColor axis = palette.axisX;
        blocks.push(PrKitTimelineBlock{.id = 3, .track = 1, .start = 2, .duration = 2, .color = axis, .label = "Turn"});
        markers.push(PrKitTimelineMarker{.track = 1, .time = 6.5f, .color = palette.danger});
        fixture.build = [this](PrLayoutNode root) {
            PrKitTimeline timeline{
                .tracks = tracks,
                .trackCount = 2,
                .blocks = blocks.data(),
                .blockCount = blocks.count(),
                .markers = markers.data(),
                .markerCount = markers.count(),
                .view = &view,
                .playhead = hasPlayhead ? &playhead : nullptr,
                .loop = hasLoop ? &loop : nullptr,
                .snap = snap,
                .readOnly = readOnly,
            };
            PrLayoutNode parent = disabled ? prKitBox(fixture.kit, root, {.disabled = true}, {}) : root;
            frame = prKitTimeline(fixture.kit, parent, timeline,
                {.trackHeight = trackHeight, .place = {.width = prKitPx(kGutter + 400), .flex = PrKitFlex_Fixed}});
            if (boxAfter)
                prKitBox(fixture.kit, root, {}, {});
            if (prKitRemoved(fixture.kit, frame)) {
                ++removals;
                Array<PrKitTimelineBlock> kept;
                for (const PrKitTimelineBlock& block : blocks) {
                    if (!block.selected)
                        kept.push(block);
                }
                blocks = kept;
            }
            uint32_t track = 0;
            float time = 0;
            if (prKitAdded(fixture.kit, frame, &track, &time)) {
                ++additions;
                blocks.push(PrKitTimelineBlock{
                    .id = 100 + additions,
                    .track = track,
                    .start = time,
                    .duration = 1,
                    .color = fixture.kit->theme.palette.accent,
                    .label = "New",
                });
            }
        };
    }

    // Returns the point at `time` seconds in the middle of `track`, or of the ruler for track -1, while the view is the
    // one that the timeline began with.
    PrVec2 at(float time, int32_t track) {
        PrRect box = fixture.bounds(frame);
        float y = track < 0 ? kRuler / 2 : kRuler + kTrack * float(track) + kTrack / 2;
        return PrVec2{box.left + kGutter + time * 50, box.top + y};
    }
};

// A script's timeline and the same timeline of the kit's own calls.
struct TimelinePair {
    JsKitScripted scripted;
    DirectTimeline direct;

    TimelinePair() {
        scripted.run(kJsKitMake);
        scripted.run(kHear);
        scripted.run(kTimelineScript);
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

TEST_CASE("a timeline that a script describes renders as the kit's timeline of the same model does") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.fixture.paintedCommands() == pair.direct.fixture.paintedCommands());
    CHECK(pair.scripted.number("heard.length") == 0);

    // The view, the loop, the height of the tracks, a selected block, and the colors, as the script sets them.
    pair.scripted.run(R"JS(
        kitTree.set(timeline, 'view', '-1 9.5');
        kitTree.set(timeline, 'loop', '2 6');
        kitTree.set(timeline, 'playhead', '3.25');
        kitTree.set(timeline, 'track-height', '34');
        kitTree.set(turn, 'selected', '');
        kitTree.set(walk, 'color', null);
        kitTree.set(run, 'color', '#cc804d80');
        kitTree.set(turn, 'color', '0.38 0.58 0.82');
        kitTree.set(cue, 'color', null);
        kitTree.set(cue, 'time', '0.5');
        kitTree.setText(rotation, 'Spin');
    )JS");
    DirectTimeline& direct = pair.direct;
    const PrKitPalette& palette = direct.fixture.kit->theme.palette;
    direct.view = PrKitTimeSpan{-1, 9.5f};
    direct.loop = PrKitTimeSpan{2, 6};
    direct.hasLoop = true;
    direct.playhead = 3.25f;
    direct.trackHeight = 34;
    direct.blocks[2].selected = true;
    direct.blocks[0].color = palette.accent;
    prKitColorParseHex("#cc804d80", &direct.blocks[1].color);
    direct.blocks[2].color = PrColor{0.38f, 0.58f, 0.82f, 1};
    direct.markers[0] = PrKitTimelineMarker{.track = 1, .time = 0.5f, .color = palette.accent};
    direct.tracks[1].name = "Spin";
    pair.frame();
    pair.frame();
    CHECK(pair.alike());

    // Without a view, a playhead, a loop and a height, the timeline shows its first ten seconds on tracks of the
    // default height. A marker takes a color by its channels as a block does, with an alpha.
    pair.scripted.run(R"JS(
        ['view', 'loop', 'playhead', 'track-height'].forEach(function (name) { kitTree.set(timeline, name, null); });
        kitTree.set(turn, 'selected', null);
        kitTree.set(cue, 'color', '0.86 0.38 0.38 0.5');
    )JS");
    direct.view = PrKitTimeSpan{0, 10};
    direct.hasLoop = false;
    direct.hasPlayhead = false;
    direct.trackHeight = 0;
    direct.blocks[2].selected = false;
    direct.markers[0].color = PrColor{0.86f, 0.38f, 0.38f, 0.5f};
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("heard.length") == 0);
}

TEST_CASE("a drag on a block of a timeline tells the script where each selected block went, and what was selected") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    pair.click(direct.at(4.5f, 0), PrAppModifierFlag_Shift);
    CHECK(direct.blocks[1].selected);
    CHECK(pair.scripted.number("count(run, 'select')") == 1);
    CHECK(pair.scripted.number("last(run, 'select').value") == 1);
    CHECK(pair.scripted.number("heard.length") == 1);

    // The press joins Walk to the selection, and the drag moves both by 1.1 s, which the step rounds to 1 s.
    pair.drag(direct.at(2, 0), direct.at(2 + 1.1f, 0), PrAppModifierFlag_Shift);
    CHECK(direct.blocks[0].start == doctest::Approx(2.0));
    CHECK(direct.blocks[1].start == doctest::Approx(5.0));
    CHECK(pair.scripted.number("last(walk, 'select').value") == 1);
    CHECK(pair.scripted.number("last(walk, 'change').values.length") == 2);
    CHECK(pair.scripted.number("last(walk, 'change').values[0]") == double(direct.blocks[0].start));
    CHECK(pair.scripted.number("last(walk, 'change').values[1]") == double(direct.blocks[0].duration));
    CHECK(pair.scripted.number("last(run, 'change').values[0]") == double(direct.blocks[1].start));
    CHECK(pair.scripted.number("count(turn, 'change') + count(turn, 'select')") == 0);
    pair.settle();
    CHECK(pair.alike());

    // What the drag left stands until the script says otherwise.
    pair.scripted.run("kitTree.set(walk, 'start', '0.5'); kitTree.set(run, 'selected', null);");
    direct.blocks[0].start = 0.5f;
    direct.blocks[1].selected = false;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
}

TEST_CASE("a drag on an end of a block of a timeline resizes it, and tells the script its start and its duration") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    pair.drag(direct.at(3 - 0.04f, 0), direct.at(3.5f, 0));
    CHECK(direct.blocks[0].duration == doctest::Approx(2.5));
    CHECK(pair.scripted.number("last(walk, 'change').values[0]") == 1);
    CHECK(pair.scripted.number("last(walk, 'change').values[1]") == double(direct.blocks[0].duration));
    pair.drag(direct.at(1 + 0.04f, 0), direct.at(0.5f, 0));
    CHECK(direct.blocks[0].start == doctest::Approx(0.5));
    CHECK(pair.scripted.number("last(walk, 'change').values[0]") == double(direct.blocks[0].start));
    CHECK(pair.scripted.number("last(walk, 'change').values[1]") == double(direct.blocks[0].duration));
    pair.settle();
    CHECK(pair.alike());

    pair.scripted.run("kitTree.set(walk, 'duration', '1');");
    direct.blocks[0].duration = 1;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
}

TEST_CASE("a drag on the ruler of a timeline scrubs the playhead, and tells the script its time") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    pair.drag(direct.at(1, -1), direct.at(6.1f, -1));
    CHECK(direct.playhead == doctest::Approx(6.0));
    CHECK(pair.scripted.number("count(timeline, 'change')") >= 1);
    CHECK(pair.scripted.number("last(timeline, 'change').value") == double(direct.playhead));
    pair.settle();
    CHECK(pair.alike());

    // A timeline with no playhead scrubs none.
    uint32_t changes = uint32_t(pair.scripted.number("count(timeline, 'change')"));
    pair.scripted.run("kitTree.set(timeline, 'playhead', null);");
    direct.hasPlayhead = false;
    pair.drag(direct.at(1, -1), direct.at(3, -1));
    CHECK(pair.scripted.number("count(timeline, 'change')") == changes);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a marquee over a timeline's lanes tells the script each block that it selects, and a click clears them") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    // From 3.5 s on the first track to 6.3 s on the second touches Run and Turn.
    pair.drag(direct.at(3.5f, 0), direct.at(6.3f, 1));
    CHECK(direct.blocks[1].selected);
    CHECK(direct.blocks[2].selected);
    CHECK(pair.scripted.number("count(walk, 'select')") == 0);
    CHECK(pair.scripted.number("last(run, 'select').value") == 1);
    CHECK(pair.scripted.number("last(turn, 'select').value") == 1);
    pair.settle();
    CHECK(pair.alike());

    pair.click(direct.at(7.5f, 1));
    CHECK_FALSE(direct.blocks[1].selected);
    CHECK(pair.scripted.number("count(run, 'select')") == 2);
    CHECK(pair.scripted.number("last(run, 'select').value") == 0);
    CHECK(pair.scripted.number("last(turn, 'select').value") == 0);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("Delete asks a timeline's script to remove the selected blocks, and a double press asks it for a block") {
    TimelinePair pair;
    pair.scripted.run(R"JS(
        function answer(node, type, detail) {
            if (type === 'remove' && node !== timeline)
                kitTree.destroy(node);
            if (type === 'add') {
                const at = {start: String(detail.values[1]), duration: '1'};
                make('timeline-block', tracks[detail.values[0]], at, 'New');
            }
        }
    )JS");
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    pair.click(direct.at(2, 0));
    CHECK(pair.scripted.number("last(walk, 'select').value") == 1);
    pair.scripted.fixture.key(PrAppKey_Delete);
    direct.fixture.key(PrAppKey_Delete);
    pair.frame();
    CHECK(direct.removals == 1);
    CHECK(direct.blocks.count() == 2);
    CHECK(pair.scripted.number("count(timeline, 'remove')") == 1);
    CHECK(pair.scripted.number("count(walk, 'remove')") == 1);
    CHECK(pair.scripted.number("heard.filter(function (event) { return event.type === 'remove'; }).length") == 2);
    // The timeline asks before the block does.
    CHECK(pair.scripted.number("heard[heard.length - 2].node === timeline ? 1 : 0") == 1);
    pair.settle();
    CHECK(pair.alike());

    PrVec2 empty = direct.at(5.6f, 1);
    KitFixture* both[] = {&pair.scripted.fixture, &direct.fixture};
    for (KitFixture* f : both) {
        f->click(empty);
        f->click(empty, 0, 2);
        f->frame();
    }
    CHECK(direct.additions == 1);
    REQUIRE(direct.blocks.count() == 3);
    CHECK(pair.scripted.number("count(timeline, 'add')") == 1);
    CHECK(pair.scripted.number("last(timeline, 'add').values.length") == 2);
    CHECK(pair.scripted.number("last(timeline, 'add').values[0]") == 1);
    // The time is snapped to the step, as a drag's is.
    CHECK(pair.scripted.number("last(timeline, 'add').values[1]") == 5.5);
    CHECK(pair.scripted.number("last(timeline, 'add').values[1]") == double(direct.blocks[2].start));
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("the wheel pans and zooms a timeline, and tells the script the view, which stands until the script sets it") {
    TimelinePair pair;
    pair.frame();
    pair.frame();
    DirectTimeline& direct = pair.direct;
    PrVec2 over = direct.at(4, 0);
    pair.scripted.fixture.move(over);
    direct.fixture.move(over);
    pair.push(PrAppEvent{.kind = PrAppEventKind_Scroll, .scroll = {.position = over, .delta = PrVec2{50, 0}}});
    CHECK(direct.view.start == doctest::Approx(1.0));
    CHECK(pair.scripted.number("count(timeline, 'view')") == 1);
    CHECK(pair.scripted.number("last(timeline, 'view').values.length") == 2);
    CHECK(pair.scripted.number("last(timeline, 'view').values[0]") == double(direct.view.start));
    CHECK(pair.scripted.number("last(timeline, 'view').values[1]") == double(direct.view.end));
    pair.push(PrAppEvent{
        .kind = PrAppEventKind_Scroll,
        .modifiers = PrAppModifierFlag_Super,
        .scroll = {.position = over, .delta = PrVec2{0, -70}},
    });
    CHECK(direct.view.end - direct.view.start < 5.0f);
    CHECK(pair.scripted.number("count(timeline, 'view')") == 2);
    CHECK(pair.scripted.number("last(timeline, 'view').values[0]") == double(direct.view.start));
    CHECK(pair.scripted.number("last(timeline, 'view').values[1]") == double(direct.view.end));
    pair.settle();
    CHECK(pair.alike());

    pair.scripted.run("kitTree.set(timeline, 'view', '2 6');");
    direct.view = PrKitTimeSpan{2, 6};
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    CHECK(pair.scripted.number("count(timeline, 'view')") == 2);
}

TEST_CASE("a drag in a timeline snaps to what the script enables: the step, block edges, markers and the playhead") {
    struct Case {
        const char* script;         // sets the snapping
        PrKitTimelineSnap snap;
        float playhead;
        PrVec2 from;                // the drag, in seconds on the first track
        PrVec2 to;
        float start;                // where Run then starts and how long it lasts
        float duration;
    };
    const Case cases[] = {
        // Run's start dragged from 4 s to 3.08 s, near Walk's end at 3 s.
        {"kitTree.set(timeline, 'snap-step', null);", {}, 0, {4.5f, 0}, {3.58f, 0}, 3.08f, 1},
        {
            "kitTree.set(timeline, 'snap-step', '0'); kitTree.set(timeline, 'snap-edges', '');",
            {.edges = true},
            0,
            {4.5f, 0},
            {3.58f, 0},
            3,
            1,
        },
        // Run's end dragged from 5 s to 6.44 s, near the marker at 6.5 s.
        {
            "kitTree.set(timeline, 'snap-step', null); kitTree.set(timeline, 'snap-markers', 'true');",
            {.markers = true},
            0,
            {4.96f, 0},
            {6.44f, 0},
            4,
            2.5f,
        },
        // Run's start dragged from 4 s to 3.3 s, near the playhead at 3.35 s.
        {
            "kitTree.set(timeline, 'snap-step', null); kitTree.set(timeline, 'snap-playhead', '');"
                "kitTree.set(timeline, 'playhead', '3.35');",
            {.playhead = true},
            3.35f,
            {4.5f, 0},
            {3.8f, 0},
            3.35f,
            1,
        },
    };
    for (const Case& c : cases) {
        TimelinePair pair;
        pair.scripted.run(c.script);
        DirectTimeline& direct = pair.direct;
        direct.snap = c.snap;
        direct.playhead = c.playhead;
        pair.frame();
        pair.frame();
        CHECK(pair.alike());
        pair.drag(direct.at(c.from.x, 0), direct.at(c.to.x, 0));
        CHECK(direct.blocks[1].start == doctest::Approx(double(c.start)).epsilon(0.002));
        CHECK(direct.blocks[1].duration == doctest::Approx(double(c.duration)).epsilon(0.002));
        CHECK(pair.scripted.number("last(run, 'change').values[0]") == double(direct.blocks[1].start));
        CHECK(pair.scripted.number("last(run, 'change').values[1]") == double(direct.blocks[1].duration));
        pair.settle();
        CHECK(pair.alike());
    }
}

TEST_CASE("a read-only timeline of a script shows its model and edits none of it") {
    TimelinePair pair;
    pair.scripted.run("kitTree.set(timeline, 'read-only', '');");
    DirectTimeline& direct = pair.direct;
    direct.readOnly = true;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    pair.drag(direct.at(2, 0), direct.at(3, 0));
    pair.drag(direct.at(1, -1), direct.at(6, -1));
    CHECK(direct.blocks[0].start == 1.0f);
    CHECK(pair.scripted.number("heard.length") == 0);
    pair.settle();
    CHECK(pair.alike());

    // Editable again, the same drag moves the block, once a frame has declared the blocks as ones that take a press.
    pair.scripted.run("kitTree.set(timeline, 'read-only', 'false');");
    direct.readOnly = false;
    pair.frame();
    pair.drag(direct.at(2, 0), direct.at(3, 0));
    CHECK(direct.blocks[0].start == doctest::Approx(2.0));
    CHECK(pair.scripted.number("last(walk, 'change').values[0]") == double(direct.blocks[0].start));
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a timeline of a script under a disabled box shows its model and edits none of it") {
    TimelinePair pair;
    pair.scripted.run("var shell = make('box', 0, {disabled: ''}); kitTree.insert(shell, timeline, 0);");
    DirectTimeline& direct = pair.direct;
    direct.disabled = true;
    pair.frame();
    pair.frame();
    CHECK(pair.alike());
    pair.drag(direct.at(2, 0), direct.at(3, 0));
    pair.drag(direct.at(1, -1), direct.at(6, -1));
    CHECK(direct.blocks[0].start == 1.0f);
    CHECK_FALSE(direct.blocks[0].selected);
    CHECK(direct.playhead == 0.0f);
    CHECK(pair.scripted.number("heard.length") == 0);
    pair.settle();
    CHECK(pair.alike());
}

TEST_CASE("a timeline's kinds refuse values that do not suit them, and declare nothing outside a timeline") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(kTimelineScript);
    scripted.run(R"JS(
        var refused = [
            kitTree.set(timeline, 'view', '4'), kitTree.set(timeline, 'view', '4 4'),
            kitTree.set(timeline, 'view', '8 0'),
            kitTree.set(timeline, 'view', '0 8 9'), kitTree.set(timeline, 'view', 'all'),
            kitTree.set(timeline, 'playhead', 'now'), kitTree.set(timeline, 'loop', '2'),
            kitTree.set(timeline, 'loop', 'a b'), kitTree.set(timeline, 'snap-step', '-1'),
            kitTree.set(timeline, 'snap-edges', 'on'), kitTree.set(timeline, 'snap-markers', '1'),
            kitTree.set(timeline, 'snap-playhead', 'yes'), kitTree.set(timeline, 'read-only', 'no'),
            kitTree.set(timeline, 'track-height', '-3'), kitTree.set(timeline, 'start', '1'),
            kitTree.set(position, 'name', 'Position'), kitTree.set(walk, 'start', 'soon'),
            kitTree.set(walk, 'duration', '-1'), kitTree.set(walk, 'color', 'blueish'),
            kitTree.set(walk, 'color', '#4d80'), kitTree.set(walk, 'color', '0.38 0.58'),
            kitTree.set(walk, 'color', '0.38 0.58 0.82 1 1'), kitTree.set(walk, 'color', '0.38 0.58 blue'),
            kitTree.set(walk, 'selected', '2'), kitTree.set(walk, 'id', '4'),
            kitTree.set(walk, 'track', '1'), kitTree.set(cue, 'time', ''), kitTree.set(cue, 'color', 'none'),
            kitTree.set(cue, 'selected', ''), kitTree.listen(timeline, 'select', true),
            kitTree.listen(position, 'add', true), kitTree.listen(walk, 'add', true),
            kitTree.listen(cue, 'change', true),
        ].filter(Boolean).length;
        var loose = make('box', 0, {});
        make('timeline-block', make('timeline-track', loose, {}, 'Loose'), {duration: '2'}, 'Under no timeline');
        make('timeline-block', loose, {duration: '2'}, 'Under no track');
        make('timeline-marker', loose, {});
        make('timeline-block', timeline, {duration: '2'}, 'A block that is on no track');
        make('label', timeline, {}, 'A label that is no track, which a timeline does not declare');
        make('label', position, {}, 'A label that is on a track, which a timeline does not declare');
    )JS");
    CHECK(scripted.number("refused") == 0);
    DirectTimeline direct;
    direct.boxAfter = true;
    scripted.fixture.frame();
    direct.fixture.frame();
    CHECK(jsKitRendersAlike(scripted.fixture, direct.fixture));
    CHECK(kitVerified(scripted.fixture.tree));
}
