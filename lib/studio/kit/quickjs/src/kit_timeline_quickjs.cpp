// The kinds of Studio Kit's timelines (pr/kit_timeline.h): `timeline`, `timeline-track`, `timeline-block` and
// `timeline-marker`.
//
// A `timeline` shows the `timeline-track` nodes under it as its tracks, in their order, under a ruler, and declares no
// other node under it. Times are in seconds. It shows no text.
//
//   view            two numbers separated by a space: the times at the start and the end of the lanes, of which the end
//                   is the later. They are 0 and 10 if it is not set
//   playhead        a number: the time of the playhead. The timeline has no playhead if it is not set
//   loop            two numbers separated by a space: the start and the end of a band that marks a loop, which is
//                   shown if its end is the later
//   snap-step       a length: the multiple that a dragged time snaps to, or zero for none
//   snap-edges      a flag: a dragged time snaps to the starts and ends of the blocks that the drag does not move, if
//                   it is within a few px of one
//   snap-markers    a flag: a dragged time snaps to the markers in the same way
//   snap-playhead   a flag: a dragged time snaps to the playhead in the same way
//   read-only       a flag: the timeline shows its model and edits none of it, as a timeline under a disabled box does
//   track-height    a length: the height of each track in px, or zero for the default
//
//   change          a drag on the ruler scrubbed the playhead. `value` is its new time, which stands until the script
//                   sets `playhead`. A timeline with no playhead scrubs none
//   view            the wheel panned or zoomed the lanes. `values` are the new start and end, which stand until the
//                   script sets `view`
//   add             a double press on an empty part of a track asked for a block. `values` are the track's index among
//                   the timeline's tracks, and the time, snapped as a dragged time is. The script puts a
//                   `timeline-block` there, or does not
//   remove          Delete or Backspace asked to remove the selection while the timeline had focus. Each selected block
//                   queues `remove` too, after the timeline
//
// A `timeline-track` is a lane of the timeline that it is under. Its text is its name, in the gutter. The
// `timeline-block` and `timeline-marker` nodes under it are on its lane. It takes no attributes of its own and queues
// no events.
//
// A `timeline-block` is a block of time on the track that it is under. Its text is its label. The timeline knows a
// block by its node, so a block takes no id.
//
//   start           a number
//   duration        a number, zero or more
//   color           one of the palette's colors by name, such as `accent` or `axis-x`, which follows the theme, or six
//                   or eight hex digits after an optional `#`. It is the accent if it is not set
//   selected        a flag
//
//   change          a drag moved the block, as it moves every selected block, or resized it from either end. `values`
//                   are its new start and duration, which stand until the script sets `start` or `duration`
//   select          the user selected or deselected the block. `value` is 1 or 0, which stands until the script sets
//                   `selected`
//   remove          its timeline asked to remove the selection, which the block is in
//
// A `timeline-marker` is a moment on the track that it is under, drawn as a diamond. The user does not edit a marker,
// and it queues no events. It shows no text.
//
//   time            a number
//   color           a color, as a block takes one
//
// A track, a block or a marker that is not under the node that holds its kind declares nothing, and its placement is
// not read.

#include "studio/kit/quickjs/src/kit_timeline_quickjs.h"

#include "pr/color.h"
#include "pr/kit_color.h"
#include "pr/kit_timeline.h"
#include "pr/raw_array.h"

namespace pr_impl {

namespace {

// The family's kinds, by their position among them.
enum {
    TimelineKind_Timeline,
    TimelineKind_Track,
    TimelineKind_Block,
    TimelineKind_Marker,
    TimelineKind_Count,
};
typedef uint32_t TimelineKind;
extern const JsKitKind kKinds[TimelineKind_Count];

// The events of a timeline and of a block, by their index.
enum { TimelineEvent_Change, TimelineEvent_View, TimelineEvent_Add, TimelineEvent_Remove };
constexpr PrStringView kTimelineEvents[] = {"change", "view", "add", "remove"};
enum { BlockEvent_Change, BlockEvent_Select, BlockEvent_Remove };
constexpr PrStringView kBlockEvents[] = {"change", "select", "remove"};

// The view of a timeline whose script set none, in seconds.
constexpr PrKitTimeSpan kDefaultView = {0, 10};

// A color that an attribute gives: one of the palette's, which follows the kit's theme, or a value of its own if
// `palette` is JsKitColor_None and `given` is true, or the accent if nothing was given.
struct TimelineColor {
    JsKitColor palette;
    bool given;
    PrColor value;
};

struct TimelineData {
    PrKitTimeSpan view;             // kDefaultView from the node's creation
    PrKitTimeSpan loop;
    float playhead;
    bool hasPlayhead;
    bool hasLoop;
    bool readOnly;
    PrKitTimelineSnap snap;
    float trackHeight;

    // The model that a round gives the kit, which the round makes from the nodes under the timeline, and the script's
    // node for each of the model's blocks, by its index there. None holds anything from round to round but its storage.
    RawArray<PrKitTimelineTrack> tracks;
    RawArray<PrKitTimelineBlock> blocks;
    RawArray<PrKitTimelineMarker> markers;
    RawArray<JsKitNodeIndex> blockSources;
};

struct BlockData {
    float start;
    float duration;
    TimelineColor color;
    bool selected;
};

struct MarkerData {
    float time;
    TimelineColor color;
};

bool isKind(const JsKitNodeTree* tree, JsKitNodeIndex node, TimelineKind kind) {
    return jsKitNodeKind(tree, node) == &kKinds[kind];
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

// Reads a span of time from two numbers separated by spaces. Returns false, and writes nothing, if `value` is anything
// else.
bool spanFrom(PrStringView value, PrKitTimeSpan* out) {
    float numbers[2] = {};
    size_t at = 0;
    PrStringView part = {};
    for (float& number : numbers) {
        if (!nextPart(value, &at, &part) || !jsKitSetFloat(part, false, &number))
            return false;
    }
    if (nextPart(value, &at, &part))
        return false;
    *out = PrKitTimeSpan{numbers[0], numbers[1]};
    return true;
}

// Reads a color: one of the palette's by name, or hex digits. With `reset`, writes that none was given.
bool setColor(PrStringView value, bool reset, TimelineColor* out) {
    TimelineColor color{};
    if (!reset) {
        if (!jsKitKeywordFrom(jsKitColors(), value, &color.palette) && !prKitColorParseHex(value, &color.value))
            return false;
        color.given = true;
    }
    *out = color;
    return true;
}

PrColor colorFrom(const PrKit& kit, const TimelineColor& color) {
    if (!color.given)
        return kit.theme.palette.accent;
    return color.palette != JsKitColor_None ? jsKitColorFrom(kit, color.palette) : color.value;
}

void timelineCreate(void* data) {
    static_cast<TimelineData*>(data)->view = kDefaultView;
}

void timelineDestroy(void* data) {
    TimelineData& timeline = *static_cast<TimelineData*>(data);
    timeline.tracks.reset();
    timeline.blocks.reset();
    timeline.markers.reset();
    timeline.blockSources.reset();
}

bool timelineSet(void* data, PrStringView name, PrStringView value, bool reset) {
    TimelineData& timeline = *static_cast<TimelineData*>(data);
    if (name == "view") {
        PrKitTimeSpan view = kDefaultView;
        if (!reset && (!spanFrom(value, &view) || !(view.end > view.start)))
            return false;
        timeline.view = view;
        return true;
    }
    if (name == "playhead") {
        if (!jsKitSetFloat(value, reset, &timeline.playhead))
            return false;
        timeline.hasPlayhead = !reset;
        return true;
    }
    if (name == "loop") {
        PrKitTimeSpan loop = {};
        if (!reset && !spanFrom(value, &loop))
            return false;
        timeline.loop = loop;
        timeline.hasLoop = !reset;
        return true;
    }
    if (name == "snap-step")
        return jsKitSetLength(value, reset, &timeline.snap.step);
    if (name == "snap-edges")
        return jsKitSetFlag(value, reset, &timeline.snap.edges);
    if (name == "snap-markers")
        return jsKitSetFlag(value, reset, &timeline.snap.markers);
    if (name == "snap-playhead")
        return jsKitSetFlag(value, reset, &timeline.snap.playhead);
    if (name == "read-only")
        return jsKitSetFlag(value, reset, &timeline.readOnly);
    if (name == "track-height")
        return jsKitSetLength(value, reset, &timeline.trackHeight);
    return false;
}

bool blockSet(void* data, PrStringView name, PrStringView value, bool reset) {
    BlockData& block = *static_cast<BlockData*>(data);
    if (name == "start")
        return jsKitSetFloat(value, reset, &block.start);
    if (name == "duration")
        return jsKitSetLength(value, reset, &block.duration);
    if (name == "color")
        return setColor(value, reset, &block.color);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &block.selected);
    return false;
}

bool markerSet(void* data, PrStringView name, PrStringView value, bool reset) {
    MarkerData& marker = *static_cast<MarkerData*>(data);
    if (name == "time")
        return jsKitSetFloat(value, reset, &marker.time);
    if (name == "color")
        return setColor(value, reset, &marker.color);
    return false;
}

// A track, a block or a marker declares nothing by itself: its timeline reads it.
PrLayoutNode partDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

// Makes the round's model from the nodes under the timeline `node`.
void timelineCollect(const JsKitNodeTree* tree, JsKitNodeIndex node, const PrKit& kit, TimelineData& timeline) {
    timeline.tracks.clear();
    timeline.blocks.clear();
    timeline.markers.clear();
    timeline.blockSources.clear();
    for (JsKitNodeIndex track = jsKitNodeFirstChild(tree, node); track != kJsKitNodeNone;
            track = jsKitNodeNextSibling(tree, track)) {
        if (!isKind(tree, track, TimelineKind_Track))
            continue;
        uint32_t index = timeline.tracks.count;
        timeline.tracks.push(PrKitTimelineTrack{.name = jsKitNodeText(tree, track)});
        for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, track); child != kJsKitNodeNone;
                child = jsKitNodeNextSibling(tree, child)) {
            if (isKind(tree, child, TimelineKind_Block)) {
                const BlockData& block = *static_cast<const BlockData*>(jsKitNodeData(tree, child));
                // A block's node is its id, which no other block has and which stays as the script moves the block.
                timeline.blocks.push(PrKitTimelineBlock{
                    .id = child,
                    .track = index,
                    .start = block.start,
                    .duration = block.duration,
                    .color = colorFrom(kit, block.color),
                    .label = jsKitNodeText(tree, child),
                    .selected = block.selected,
                });
                timeline.blockSources.push(child);
            } else if (isKind(tree, child, TimelineKind_Marker)) {
                const MarkerData& marker = *static_cast<const MarkerData*>(jsKitNodeData(tree, child));
                timeline.markers.push(PrKitTimelineMarker{
                    .track = index,
                    .time = marker.time,
                    .color = colorFrom(kit, marker.color),
                });
            }
        }
    }
}

PrLayoutNode timelineDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    TimelineData& timeline = jsKitData<TimelineData>(round, node);
    const JsKitNodeTree* tree = round.tree;
    PrKit* kit = round.kit;
    timelineCollect(tree, node, *kit, timeline);
    PrKitTimeSpan view = timeline.view;
    float playhead = timeline.playhead;
    PrKitTimeline content{
        .key = jsKitNodeKey(round, node),
        .tracks = timeline.tracks.data,
        .trackCount = timeline.tracks.count,
        .blocks = timeline.blocks.data,
        .blockCount = timeline.blocks.count,
        .markers = timeline.markers.data,
        .markerCount = timeline.markers.count,
        .view = &timeline.view,
        .playhead = timeline.hasPlayhead ? &timeline.playhead : nullptr,
        .loop = timeline.hasLoop ? &timeline.loop : nullptr,
        .snap = timeline.snap,
        .readOnly = timeline.readOnly,
    };
    PrKitTimelineStyle style{.trackHeight = timeline.trackHeight, .place = jsKitNodePlace(tree, node)};
    PrLayoutNode made = prKitTimeline(kit, parent, content, style);

    if (timeline.playhead != playhead) {
        JsKitDetail detail{.values = {double(timeline.playhead)}, .valueCount = 1};
        jsKitEventPush(round, node, TimelineEvent_Change, &detail);
    }
    if (timeline.view.start != view.start || timeline.view.end != view.end) {
        JsKitDetail detail{.values = {double(timeline.view.start), double(timeline.view.end)}, .valueCount = 2};
        jsKitEventPush(round, node, TimelineEvent_View, &detail);
    }
    uint32_t track = 0;
    float time = 0;
    if (prKitAdded(kit, made, &track, &time)) {
        JsKitDetail detail{.values = {double(track), double(time)}, .valueCount = 2};
        jsKitEventPush(round, node, TimelineEvent_Add, &detail);
    }
    bool removed = prKitRemoved(kit, made);
    if (removed)
        jsKitEventPush(round, node, TimelineEvent_Remove, nullptr);

    // What the kit wrote to the round's blocks goes into the nodes that they were made from.
    for (uint32_t i = 0; i < timeline.blocks.count; ++i) {
        const PrKitTimelineBlock& edited = timeline.blocks.data[i];
        JsKitNodeIndex source = timeline.blockSources.data[i];
        BlockData& block = *static_cast<BlockData*>(jsKitNodeData(tree, source));
        if (edited.start != block.start || edited.duration != block.duration) {
            block.start = edited.start;
            block.duration = edited.duration;
            JsKitDetail detail{.values = {double(edited.start), double(edited.duration)}, .valueCount = 2};
            jsKitEventPush(round, source, BlockEvent_Change, &detail);
        }
        if (edited.selected != block.selected) {
            block.selected = edited.selected;
            JsKitDetail detail{.values = {edited.selected ? 1.0 : 0.0}, .valueCount = 1};
            jsKitEventPush(round, source, BlockEvent_Select, &detail);
        }
        if (removed && edited.selected)
            jsKitEventPush(round, source, BlockEvent_Remove, nullptr);
    }
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[TimelineKind_Count] = {
    {
        .name = "timeline",
        .dataSize = sizeof(TimelineData),
        .events = kTimelineEvents,
        .eventCount = 4,
        .flags = JsKitKindFlag_PlacesItself,
        .create = timelineCreate,
        .destroy = timelineDestroy,
        .set = timelineSet,
        .declare = timelineDeclare,
    },
    {.name = "timeline-track", .declare = partDeclare},
    {
        .name = "timeline-block",
        .dataSize = sizeof(BlockData),
        .events = kBlockEvents,
        .eventCount = 3,
        .set = blockSet,
        .declare = partDeclare,
    },
    {.name = "timeline-marker", .dataSize = sizeof(MarkerData), .set = markerSet, .declare = partDeclare},
};

} // namespace

Span<const JsKitKind> jsKitTimelineKinds() {
    return kKinds;
}

} // namespace pr_impl
