// The kinds of Studio Kit's readouts in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_data.h"

#include "studio/kit/quickjs/tests/kit_scripted_quickjs.h"

#include "tools/harness/test.h"

using namespace harness;

namespace pr_impl {

namespace {

// Runs `count` frames of both kits.
void frames(JsKitScripted& scripted, KitFixture& direct, uint32_t count = 1) {
    for (uint32_t i = 0; i < count; ++i) {
        scripted.fixture.frame();
        direct.frame();
    }
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("a stat tile shows its caption, its reading and its note, and follows the script's edits of them") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '8', padding: '8', align: 'start'});
        var frame = make('stat-tile', row, {value: '16.6 ms', note: '+0.4 ms', 'note-tone': 'danger'}, 'FRAME TIME');
        make('stat-tile', row, {value: '1,204'}, 'DRAW CALLS');
        var memory = make('stat-tile', row, {value: '412 MB', note: 'of 2 GB', margin: '0 0 0 16'}, 'MEMORY');
        make('badge', memory, {variant: 'warning'}, 'high');
    )JS");
    KitFixture direct;
    PrKitStatTile frame{.label = "FRAME TIME", .value = "16.6 ms", .note = "+0.4 ms", .noteTone = PrKitTone_Danger};
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Start,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        });
        prKitStatTile(kit, row, frame);
        prKitStatTile(kit, row, {.label = "DRAW CALLS", .value = "1,204"});
        PrLayoutNode memory = prKitStatTile(kit, row, {.label = "MEMORY", .value = "412 MB", .note = "of 2 GB"});
        prKitPlace(kit, memory, {.margin = PrKitEdges{0, 0, 0, 16}});
        prKitBadge(kit, memory, {.text = "high"}, {.variant = PrKitBadgeVariant_Warning});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    scripted.run(R"JS(
        kitTree.setText(frame, 'FRAME');
        kitTree.set(frame, 'value', '8.3 ms');
        kitTree.set(frame, 'note', '-8.3 ms');
        kitTree.set(frame, 'note-tone', 'success');
    )JS");
    frame = PrKitStatTile{.label = "FRAME", .value = "8.3 ms", .note = "-8.3 ms", .noteTone = PrKitTone_Success};
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // Reset, a tile has no note, and its reading is empty.
    scripted.run("kitTree.set(frame, 'note', null); kitTree.set(frame, 'note-tone', null); kitTree.set(frame, 'value', null);");
    frame = PrKitStatTile{.label = "FRAME"};
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a log line shows its time, the tag of its level, its message and its source") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var log = make('box', 0, {width: '480'});
        make('log-line', log, {time: '12:04:01', source: 'renderer'}, 'Compiled 14 shaders');
        var line = make('log-line', log, {time: '12:04:02', source: 'assets', level: 'warning'}, 'Texture is not a power of two');
        make('log-line', log, {level: 'error'}, 'A message that is too long for its line is cut where the line ends, at its source');
        var held = make('log-line', log, {level: 'info', source: 'ui', margin: '8 0'}, 'A line holds what is under it');
        make('badge', held, {}, 'new');
    )JS");
    KitFixture direct;
    PrKitLogLine line{
        .time = "12:04:02",
        .message = "Texture is not a power of two",
        .source = "assets",
        .level = PrKitLogLevel_Warning,
    };
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode log = prKitBox(kit, root, {}, {.place = {.width = prKitPx(480)}});
        prKitLogLine(kit, log, {.time = "12:04:01", .message = "Compiled 14 shaders", .source = "renderer"});
        prKitLogLine(kit, log, line);
        prKitLogLine(kit, log, {
            .message = "A message that is too long for its line is cut where the line ends, at its source",
            .level = PrKitLogLevel_Error,
        });
        PrLayoutNode held = prKitLogLine(kit, log, {.message = "A line holds what is under it", .source = "ui"});
        prKitPlace(kit, held, {.margin = prKitEdgesXY(8, 0)});
        prKitBadge(kit, held, {.text = "new"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    const char* levels[] = {"info", "warning", "error"};
    for (uint32_t level = 0; level < 3; ++level) {
        CAPTURE(levels[level]);
        String source("kitTree.set(line, 'level', '");
        source.append(levels[level]).append("');").append('\0');
        scripted.run(source.data());
        line.level = PrKitLogLevel(level);
        frames(scripted, direct);
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }

    // A level that is not one is refused, and reset, a line has no time, no source and the level of information.
    scripted.run("var refused = kitTree.set(line, 'level', 'debug');");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    scripted.run(R"JS(
        kitTree.set(line, 'time', null);
        kitTree.set(line, 'source', null);
        kitTree.set(line, 'level', null);
        kitTree.setText(line, 'Texture resized');
    )JS");
    line = PrKitLogLine{.message = "Texture resized"};
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("the kinds of readouts take only their own attributes, and no events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var tile = kitTree.create('stat-tile');
        var line = kitTree.create('log-line');
        var taken = [
            kitTree.set(tile, 'value', ''), kitTree.set(tile, 'note', 'a note'), kitTree.set(tile, 'note-tone', 'faint'),
            kitTree.set(tile, 'min-width', '120'), kitTree.set(line, 'time', '00:00'), kitTree.set(line, 'source', ''),
            kitTree.set(line, 'level', 'error'),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(tile, 'label', 'FPS'), kitTree.set(tile, 'tone', 'accent'), kitTree.set(tile, 'note-tone', 'loud'),
            kitTree.set(tile, 'level', 'error'), kitTree.set(line, 'level', 'warn'), kitTree.set(line, 'level', ''),
            kitTree.set(line, 'message', 'text'), kitTree.set(line, 'value', '1'), kitTree.listen(tile, 'click', true),
            kitTree.listen(line, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 7);
    CHECK(scripted.number("refused") == 0);
}
