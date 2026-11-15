// The host of a Svelte application that renders with Studio Kit (tools/svelte/src/kit.js).
//
// `svelte_kit_host <bundle.js>` runs the bundle in a window. The bundle is one classic script, which build.js writes
// for the kit backend. It runs in the realm of a page that is never shown, so that the application's own script has the
// web's timers, fetch and console, and it makes its widgets through `kitTree` (pr/kit_quickjs.h), which each round of
// the window declares.
//
// --headless runs the bundle with no window and no device, in a kit of Studio's dark theme: a first frame, then each
// --click <x>,<y>, a press and a release at a point of the viewport in px, in a frame of its own, then the frames that
// let the kit's transitions end. --screenshot <png> writes what the last frame shows, rendered on the CPU. --extent
// <width>x<height> sets the viewport, 720x480 by default. The frames run on a clock of their own, so a run depends on
// nothing but the bundle.
//
// --native <name>, in place of a bundle, runs a program that declares its widgets with the kit's own calls, which is
// what a bundle ported from that program is compared with. The one program is `hello_style`.

#include "pr/array.h"
#include "pr/callback.h"
#include "pr/compress.h"
#include "pr/count_of.h"
#include "pr/dom.h"
#include "pr/draw_glyph.h"
#include "pr/draw_list.h"
#include "pr/draw_renderer.h"
#include "pr/font_bundle.h"
#include "pr/font_set.h"
#include "pr/format.h"
#include "pr/html_console_quickjs.h"
#include "pr/html_page.h"
#include "pr/html_quickjs.h"
#include "pr/image.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_panel.h"
#include "pr/kit_quickjs.h"
#include "pr/kit_text.h"
#include "pr/layout_frame.h"
#include "pr/layout_scene.h"
#include "pr/layout_tree.h"
#include "pr/layout_unit.h"
#include "pr/layout_value.h"
#include "pr/print.h"
#include "pr/studio_app.h"
#include "pr/text_buffer.h"
#include "pr/text_harfbuzz.h"
#include "pr/time.h"
#include "pr/ui.h"
#include "pr/ui_records.h"

#include "tools/harness/harness.h"
#include "tools/harness/harness_png.h"

namespace {

using namespace harness;
using namespace pr;

// ---------------------------------------------------------------------------------------------------------------------
// The script

// The realm that runs a bundle, and the tree that the bundle edits.
struct ScriptHost {
    PrJsHtmlEngine* PR_NULLABLE engine = nullptr;
    PrHtmlPage* PR_NULLABLE page = nullptr;
    PrJsKitTree* PR_NULLABLE tree = nullptr;

    // The time that the page's timers read, in milliseconds, if `virtualClock` is true. A headless run advances it with
    // its frames. A window's run reads the system's clock instead.
    bool virtualClock = false;
    double virtualNow = 0;
};

double scriptNow(void* PR_NULLABLE context) {
    const ScriptHost* script = static_cast<const ScriptHost*>(context);
    return script->virtualClock ? script->virtualNow : prTimeMonotonicMilliseconds();
}

// Prints the console's messages: errors and warnings on stderr, and the rest on stdout.
void consolePrint(JSContext*, PrJsHtmlConsoleLevel level, uint32_t depth, PrStringView text) {
    for (uint32_t i = 0; i < depth; ++i)
        print("  ");
    if (level == PrJsHtmlConsoleLevel_Error || level == PrJsHtmlConsoleLevel_Warn)
        printError("{}\n", text);
    else
        print("{}\n", text);
}

constexpr PrJsHtmlConsolePrinter kConsole = {consolePrint};

// Runs the bundle at `path` in a new realm. Returns false, saying why on stderr, if the file cannot be read or the
// script throws.
bool scriptStart(ScriptHost& script, const char* path) {
    PrTextBuffer source{};
    if (!harnessReadFile(&source, path)) {
        printError("svelte_kit_host: {} cannot be read\n", PrStringView(path));
        return false;
    }
    PrJsHtmlEngineDesc desc{.console = &kConsole};
    script.engine = prJsHtmlEngineCreate(&desc);
    PrHtmlHost host = prHtmlHostDefault();
    host.context = &script;
    host.now = scriptNow;
    PrHtmlHost scripted = prJsHtmlEngineHost(script.engine, &host, PrJsHtmlScripting_Enabled);
    script.page = prHtmlPageCreate(&scripted);
    script.tree = prJsKitTreeCreate();
    JSContext* ctx = prJsHtmlEngineRealm(script.engine, script.page);
    bool ran = ctx != nullptr;
    if (ran) {
        prJsKitTreeBind(script.tree, ctx);
        ran = prJsHtmlEngineRun(ctx, PrStringView{source.data, source.size}, PrStringView(path), 1, 1);
    }
    prTextBufferReset(&source);
    if (!ran)
        printError("svelte_kit_host: {} did not run\n", PrStringView(path));
    return ran;
}

// Destroys the realm before the page and the tree, which its objects refer to.
void scriptStop(ScriptHost& script) {
    prJsHtmlEngineDestroy(script.engine);
    prHtmlPageDestroy(script.page);
    prJsKitTreeDestroy(script.tree);
    script = ScriptHost{};
}

// Runs the tasks of the script that are due: its timers, and what its promises and fetches settled.
void scriptPump(ScriptHost& script) {
    PrDomAgent* agent = prHtmlPageAgent(script.page);
    while (prDomAgentNextTaskDelay(agent) == 0 && prDomAgentRun(agent, 4)) {
    }
}

// Declares the script's widgets under `root`, then tells the script of the round's input. What the script changes in
// answer is declared by the next round, which the context runs after a round that routed a press or a release.
void scriptBuild(ScriptHost& script, PrKit* kit, PrLayoutNode root) {
    prJsKitTreeBuild(script.tree, kit, root);
    if (prJsKitTreeDispatch(script.tree))
        prJsHtmlEngineCheckpoint(script.engine);
}

// ---------------------------------------------------------------------------------------------------------------------
// The native programs

// examples/hello_style, declared as its build declares it, for a kit of any host. Its recipes are the example's.

struct CardBox {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Box;
    void make(const PrKit&, PrLayoutBoxStyle& box) const {
        box.display = PrLayoutDisplay_Block;
        for (uint32_t side = 0; side < PrRectSide_Count; ++side) {
            box.border[side] = prLayoutUnitFromPx(2);
            box.padding[side] = prLayoutLengthPx(16);
        }
    }
};

struct CardPaint {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Decoration;
    void make(const PrKit& kit, PrLayoutDecorationStyle& decoration) const {
        decoration.background = prColor16FromColor(kit.theme.palette.accentWeak);
        for (uint32_t side = 0; side < PrRectSide_Count; ++side) {
            decoration.borderColor[side] = prColor16FromColor(kit.theme.palette.accent);
            decoration.borderStyle[side] = PrDrawBorderStyle_Solid;
        }
        for (PrLayoutLength& radius : decoration.radii)
            radius = prLayoutLengthPx(12);
    }
};

struct CardType {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Font;
    void make(const PrKit& kit, PrLayoutFontStyle& font) const {
        font.font = kit.fonts[PrKitTextRole_Heading];
    }
};

struct CardInk {
    static constexpr PrLayoutGroup kKind = PrLayoutGroup_Color;
    void make(const PrKit& kit, PrLayoutColorStyle& color) const {
        color.color = prColor16FromColor(kit.theme.palette.text1);
    }
};

void helloStyleBuild(PrKit* kit, PrLayoutNode root, uint32_t& clicks) {
    PrKitBoxStyle column{.align = PrKitAlign_Start, .gap = kPrKitSpace8, .padding = prKitEdgesAll(kPrKitSpace16)};
    PrLayoutNode page = prKitBox(kit, root, {}, column);
    prKitLabel(kit, page, {.text = "Hello, world"}, {.role = PrKitTextRole_Display});
    prKitLabel(kit, page, {.text = "Kit text names a role and a tone, and the theme gives the face and the color."},
        {.tone = PrKitTone_Secondary});

    PrLayoutNode row = prKitBox(kit, page, {}, {
        .direction = PrKitDirection_Row,
        .align = PrKitAlign_Center,
        .gap = kPrKitSpace4,
    });
    PrLayoutNode button = prKitButton(kit, row, {.text = "Click me"},
        {.variant = PrKitButtonVariant_Primary, .size = PrKitControlSize_Lg});
    if (prKitActivated(kit, button))
        ++clicks;
    PrKitBadgeStyle badge{};
    if (clicks > 0)
        badge.variant = PrKitBadgeVariant_Accent;
    char count[32];
    prKitBadge(kit, row, {.text = formatToBuffer(Span(count), "{} clicks", clicks)}, badge);

    PrLayoutNode note = prKitLabel(kit, page, {
        .text = "This label is placed by its caller, at 320 px wide, so it wraps onto a second line.",
    }, {});
    prKitPlace(kit, note, {.width = prKitPx(320)});

    PrLayoutStyleIndex card = uiStyle(kit->ui, *kit, CardBox{}, CardPaint{}, CardType{}, CardInk{});
    PrLayoutNode node = prLayoutTreeFrameElement(kit->tree, page, 0, card);
    prLayoutTreeFrameText(kit->tree, node, 0, "A node styled with lib/layout's groups");
}

// ---------------------------------------------------------------------------------------------------------------------
// What a run declares

// A run's content: a bundle's script, or a native program and its model.
struct Content {
    ScriptHost script;
    bool native = false;
    uint32_t clicks = 0;
};

void contentBuild(Content& content, PrKit* kit, PrLayoutNode root) {
    if (content.native)
        helloStyleBuild(kit, root, content.clicks);
    else
        scriptBuild(content.script, kit, root);
}

// ---------------------------------------------------------------------------------------------------------------------
// Headless runs

// A point of the viewport that a headless run presses and releases, in px.
struct Click {
    PrVec2 at;
};

// The number of frames that a headless run gives the kit's transitions to end, a sixtieth of a second apart.
constexpr uint32_t kSettleFrames = 60;

// A kit with a context and a tree of its own, which nothing shows. Its frames declare a root panel on the application's
// background, as a Studio window's do, and then the run's content.
struct Headless {
    PrFontSet* fonts;
    PrTextShaper* shaper;
    PrLayoutTree* tree;
    PrUi* ui;
    PrKit* kit;
    PrUVec2 extent;
    double time = 0;
};

bool headlessCreate(Headless& headless, PrUVec2 extent) {
    headless.extent = extent;
    headless.fonts = prFontSetCreate(nullptr);
    const PrFontBundle bundles[] = {PrFontBundle_Standard14};
    if (prFontBundleInstall(headless.fonts, bundles, uint32_t(countOf(bundles)), PR_FONT_BUNDLE_DIR) == 0) {
        printError("svelte_kit_host: the fonts under {} could not be installed\n", PR_FONT_BUNDLE_DIR);
        prFontSetDestroy(headless.fonts);
        return false;
    }
    headless.shaper = prTextHarfBuzzCreate();
    PrLayoutTreeDesc desc{.shaper = headless.shaper, .targetPixelRatio = 1};
    headless.tree = prLayoutTreeCreate(&desc);
    headless.ui = prUiCreate(headless.tree);
    PrKitTheme theme = prKitThemeStudio(PrKitThemeVariant_Dark, headless.fonts);
    headless.kit = prKitCreate(headless.ui, &theme);
    return true;
}

void headlessDestroy(Headless& headless) {
    prKitDestroy(headless.kit);
    prUiDestroy(headless.ui);
    prLayoutTreeDestroy(headless.tree);
    prTextShaperDestroy(headless.shaper);
    prFontSetDestroy(headless.fonts);
}

// Runs a frame a sixtieth of a second after the last, with the script's due tasks first, as a window's frame runs.
void headlessFrame(Headless& headless, Content& content) {
    const double delta = 1.0 / 60;
    headless.time += delta;
    if (!content.native) {
        content.script.virtualNow = headless.time * 1000;
        scriptPump(content.script);
    }
    PrUiFrameDesc desc{
        .time = headless.time,
        .delta = delta,
        .extent = PrVec2{float(headless.extent.x), float(headless.extent.y)},
        .maxRounds = 4,
        .display = prAppDisplayDefault(),
        .preferences = prAppUserPreferencesDefault(),
    };
    auto round = [&](PrUi*, const PrUiRound*) {
        PrKitPanelStyle style{
            .box = {.overflow = PrKitOverflow_Clip, .place = {.width = prKitPercent(100), .height = prKitPercent(100)}},
            .background = PrKitBackground_App,
        };
        PrLayoutNode root = prKitPanel(headless.kit, kPrLayoutNodeNone, {}, style);
        contentBuild(content, headless.kit, root);
    };
    prUiFrame(headless.ui, &desc, PR_SCOPED_CALLBACK(round));
}

void headlessClick(Headless& headless, PrVec2 at) {
    PrAppEvent move{.kind = PrAppEventKind_PointerMove, .time = headless.time, .pointer = {.position = at}};
    PrAppEvent down{.kind = PrAppEventKind_PointerDown, .time = headless.time, .pointer = {.position = at, .clicks = 1}};
    PrAppEvent up{.kind = PrAppEventKind_PointerUp, .time = headless.time, .pointer = {.position = at}};
    prUiPushEvent(headless.ui, kPrUiViewportMain, &move);
    prUiPushEvent(headless.ui, kPrUiViewportMain, &down);
    prUiPushEvent(headless.ui, kPrUiViewportMain, &up);
}

// Renders the tree as the last frame laid it out, on the CPU, over the color that a window would clear to, and writes
// it to `path` as a PNG. Returns false if it cannot.
bool headlessScreenshot(Headless& headless, const char* path) {
    PrCompressCodec* compress = prCompressCodecCreate(PrCompressCodecBackend_Default);
    PrImageCodec* images = prImageCodecCreate(PrImageCodecBackend_Default, compress);
    PrDrawGlyphDecoder* glyphs = prDrawGlyphBitmapCreate(images);
    PrDrawRendererOptions rendererOptions = prDrawRendererOptionsDefault();
    rendererOptions.glyphDecoder = glyphs;
    PrDrawRenderer* renderer = nullptr;
    bool written = false;
    if (prDrawRendererCreate(PrDrawRendererBackend_Cpu, nullptr, &rendererOptions, &renderer) == PrStatus_Ok) {
        PrDrawList* list = prDrawListCreate();
        const PrLayoutScene* scene = prLayoutTreeScene(headless.tree);
        PrColor canvas = prColorFromColor16(prLayoutSceneCanvasColor(scene));
        Array<PrLayoutSceneScroll> scrolls(prLayoutSceneScrolls(headless.tree, scene, nullptr, 0));
        prLayoutSceneScrolls(headless.tree, scene, scrolls.data(), scrolls.count());
        PrLayoutScenePlayOptions options = prLayoutScenePlayOptionsDefault();
        options.scrolls = scrolls.data();
        options.scrollCount = scrolls.count();
        options.canvasFilled = true;
        prLayoutScenePlay(scene, list, &options);
        prLayoutSceneRelease(scene);

        // Rgba8 holds premultiplied values.
        Array<uint8_t> rgba;
        rgba.resize(headless.extent.x * headless.extent.y * 4);
        auto channel = [&](float value) { return uint8_t(value * canvas.a * 255 + 0.5f); };
        const uint8_t clear[4] = {channel(canvas.r), channel(canvas.g), channel(canvas.b), uint8_t(canvas.a * 255 + 0.5f)};
        for (uint32_t i = 0; i < rgba.count(); ++i)
            rgba[i] = clear[i % 4];
        size_t stride = size_t(headless.extent.x) * 4;
        PrPixels pixels{rgba.data(), stride, headless.extent};
        written = prDrawRendererRenderPixels(renderer, list, pixels, PrPixelFormat_Rgba8) == PrStatus_Ok &&
            harnessWritePng(path, PrPixelView{rgba.data(), stride, headless.extent});
        prDrawListRelease(list);
        prDrawRendererDestroy(renderer);
    }
    prDrawGlyphDecoderDestroy(glyphs);
    prImageCodecDestroy(images);
    prCompressCodecDestroy(compress);
    return written;
}

int runHeadless(Content& content, PrUVec2 extent, const Array<Click>& clicks, const char* PR_NULLABLE screenshot) {
    Headless headless{};
    if (!headlessCreate(headless, extent))
        return 1;
    // The first frame of a context routes no input, and lays out what the clicks then hit.
    headlessFrame(headless, content);
    for (const Click& click : clicks) {
        headlessClick(headless, click.at);
        headlessFrame(headless, content);
    }
    for (uint32_t i = 0; i < kSettleFrames; ++i)
        headlessFrame(headless, content);
    int status = 0;
    if (screenshot != nullptr && !headlessScreenshot(headless, screenshot)) {
        printError("svelte_kit_host: {} could not be written\n", PrStringView(screenshot));
        status = 1;
    }
    headlessDestroy(headless);
    return status;
}

// ---------------------------------------------------------------------------------------------------------------------
// Windows

// A window's content, and the time of its last frame, in seconds on the clock of the frames.
struct Windowed {
    Content* content;
    double time = 0;
};

void windowBeginFrame(void* context, PrStudioWindow*, const PrAppFrame* frame) {
    Windowed* windowed = static_cast<Windowed*>(context);
    windowed->time = frame->time;
    if (!windowed->content->native)
        scriptPump(windowed->content->script);
}

void windowBuild(void* context, PrStudioWindow* window, PrLayoutNode root, PrVec2) {
    contentBuild(*static_cast<Windowed*>(context)->content, prStudioWindowKit(window), root);
}

// Asks for the frame that the script's next task is due in, so that a timer runs with no input to wake the window.
bool windowLaidOut(void* context, PrStudioWindow* window) {
    Windowed* windowed = static_cast<Windowed*>(context);
    if (windowed->content->native)
        return false;
    double delay = prDomAgentNextTaskDelay(prHtmlPageAgent(windowed->content->script.page));
    if (delay == 0)
        return true;
    if (delay > 0)
        prUiRequestFrameAt(prStudioWindowUi(window), windowed->time + delay / 1000);
    return false;
}

int runWindow(Content& content, PrUVec2 extent) {
    PrFontSet* fonts = prFontSetCreate(nullptr);
    const PrFontBundle bundles[] = {PrFontBundle_Standard14};
    if (prFontBundleInstall(fonts, bundles, uint32_t(countOf(bundles)), PR_FONT_BUNDLE_DIR) == 0) {
        printError("svelte_kit_host: the fonts under {} could not be installed\n", PR_FONT_BUNDLE_DIR);
        prFontSetDestroy(fonts);
        return 1;
    }
    Windowed windowed{.content = &content};
    PrStudioWindowDesc desc{
        .title = "Svelte",
        .extent = extent,
        .beginFrame = windowBeginFrame,
        .build = windowBuild,
        .laidOut = windowLaidOut,
        .context = &windowed,
    };
    PrStudioAppDesc app{.fonts = fonts, .window = &desc};
    PrStatus status = prStudioAppRun(&app);
    prFontSetDestroy(fonts);
    if (status == PrStatus_NotSupported)
        printError("svelte_kit_host: no GPU device to render with\n");
    return status == PrStatus_Ok ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// Arguments

// Reads two whole numbers around `separator`, as in 720x480 or 120,40. Returns false for anything else.
bool readPair(PrStringView text, char separator, uint32_t* first, uint32_t* second) {
    uint32_t values[2] = {};
    uint32_t digits[2] = {};
    uint32_t part = 0;
    for (size_t i = 0; i < text.size; ++i) {
        char c = text.data[i];
        if (c == separator && part == 0) {
            part = 1;
        } else if (c >= '0' && c <= '9' && digits[part] < 6) {
            values[part] = values[part] * 10 + uint32_t(c - '0');
            ++digits[part];
        } else {
            return false;
        }
    }
    *first = values[0];
    *second = values[1];
    return digits[0] > 0 && digits[1] > 0;
}

int usage() {
    printError("usage: svelte_kit_host [--headless [--click <x>,<y>]... [--screenshot <png>]] "
        "[--extent <width>x<height>] <bundle.js | --native hello_style>\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    prCrashHandler();
    bool headless = false;
    PrUVec2 extent = {720, 480};
    Array<Click> clicks;
    const char* screenshot = nullptr;
    const char* bundle = nullptr;
    Content content;
    for (int i = 1; i < argc; ++i) {
        PrStringView argument = argv[i];
        bool hasValue = i + 1 < argc;
        if (argument == "--headless") {
            headless = true;
        } else if (argument == "--click" && hasValue) {
            uint32_t x = 0;
            uint32_t y = 0;
            if (!readPair(argv[++i], ',', &x, &y))
                return usage();
            clicks.push(Click{PrVec2{float(x), float(y)}});
        } else if (argument == "--extent" && hasValue) {
            if (!readPair(argv[++i], 'x', &extent.x, &extent.y) || extent.x == 0 || extent.y == 0)
                return usage();
        } else if (argument == "--screenshot" && hasValue) {
            screenshot = argv[++i];
        } else if (argument == "--native" && hasValue) {
            if (PrStringView(argv[++i]) != "hello_style")
                return usage();
            content.native = true;
        } else if (argument.size > 0 && argument.data[0] != '-' && bundle == nullptr) {
            bundle = argv[i];
        } else {
            return usage();
        }
    }
    if (content.native == (bundle != nullptr) || (!headless && (clicks.count() > 0 || screenshot != nullptr)))
        return usage();
    content.script.virtualClock = headless;
    if (!content.native && !scriptStart(content.script, bundle)) {
        scriptStop(content.script);
        return 1;
    }
    int status = headless ? runHeadless(content, extent, clicks, screenshot) : runWindow(content, extent);
    if (!content.native)
        scriptStop(content.script);
    return status;
}
