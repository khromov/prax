// The kinds of Studio Kit's readouts (pr/kit_data.h): `stat-tile` and `log-line`.
//
// Neither kind queues an event, and the nodes under either are declared in its widget, after the widget's own parts. A
// stat tile and what is under it take no input.
//
// A `stat-tile` (prKitStatTile) shows its node's text as its caption, PrKitStatTile's `label`. It takes `value`, a
// string, which is the reading, `note`, a string, and `note-tone`, a keyword of PrKitTone.
//
// A `log-line` (prKitLogLine) shows its node's text as its message. It takes `time` and `source`, strings, and `level`,
// a keyword of PrKitLogLevel.

#include "studio/kit/quickjs/src/kit_data_quickjs.h"

#include "pr/kit_data.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kLogLevels[] = {
    {"info", PrKitLogLevel_Info},
    {"warning", PrKitLogLevel_Warning},
    {"error", PrKitLogLevel_Error},
};

struct StatTileData {
    PrRawString value;
    PrRawString note;
    PrKitTone noteTone;
};

struct LogLineData {
    PrRawString time;
    PrRawString source;
    PrKitLogLevel level;
};

void statTileDestroy(void* data) {
    StatTileData& tile = *static_cast<StatTileData*>(data);
    prRawStringReset(&tile.value);
    prRawStringReset(&tile.note);
}

bool statTileSet(void* data, PrStringView name, PrStringView value, bool reset) {
    StatTileData& tile = *static_cast<StatTileData*>(data);
    if (name == "value")
        return jsKitSetString(value, reset, &tile.value);
    if (name == "note")
        return jsKitSetString(value, reset, &tile.note);
    if (name == "note-tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &tile.noteTone);
    return false;
}

PrLayoutNode statTileDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const StatTileData& tile = jsKitData<StatTileData>(round, node);
    return prKitStatTile(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .label = jsKitNodeText(round.tree, node),
        .value = prRawStringView(&tile.value),
        .note = prRawStringView(&tile.note),
        .noteTone = tile.noteTone,
    });
}

void logLineDestroy(void* data) {
    LogLineData& line = *static_cast<LogLineData*>(data);
    prRawStringReset(&line.time);
    prRawStringReset(&line.source);
}

bool logLineSet(void* data, PrStringView name, PrStringView value, bool reset) {
    LogLineData& line = *static_cast<LogLineData*>(data);
    if (name == "time")
        return jsKitSetString(value, reset, &line.time);
    if (name == "source")
        return jsKitSetString(value, reset, &line.source);
    if (name == "level")
        return jsKitSetKeyword(kLogLevels, value, reset, &line.level);
    return false;
}

PrLayoutNode logLineDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const LogLineData& line = jsKitData<LogLineData>(round, node);
    return prKitLogLine(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .time = prRawStringView(&line.time),
        .message = jsKitNodeText(round.tree, node),
        .source = prRawStringView(&line.source),
        .level = line.level,
    });
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "stat-tile",
        .dataSize = sizeof(StatTileData),
        .destroy = statTileDestroy,
        .set = statTileSet,
        .declare = statTileDeclare,
    },
    {
        .name = "log-line",
        .dataSize = sizeof(LogLineData),
        .destroy = logLineDestroy,
        .set = logLineSet,
        .declare = logLineDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitDataKinds() {
    return kKinds;
}

} // namespace pr_impl
