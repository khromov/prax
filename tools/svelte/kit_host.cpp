// The host of a Svelte application that renders with Studio Kit (tools/svelte/src/kit.js).
//
// `svelte_kit_host <bundle.js>` runs the bundle in a window. The bundle is one classic script, which build.js writes
// for the kit backend. It runs in the realm of a page that is never shown, so that the application's own script has the
// web's timers, fetch and console, and it makes its widgets through `kitTree` (pr/kit_quickjs.h), which each round of
// the window declares. Each --arg <name>=<value> is a property of the script's `hostArguments`.
//
// --native <program>, in place of a bundle, runs a program that declares its widgets with the kit's own calls, which is
// what a bundle ported from that program is compared with: `hello_style`, `gallery`, which is examples/studio_gallery
// whole, or `gallery:<section>`, one of its sections by its name in lower case, alone on the gallery's page.
//
// --headless runs with no window and no device, in a kit of Studio's dark theme, on a clock of its own, so that a run
// depends on nothing but its content and its input. It runs a first frame, then its input, then writes --screenshot
// <png>, what the last frame shows, rendered on the CPU. --extent <width>x<height> sets the viewport, 720x480 by
// default.
//
// The input is each --click <x>,<y>, a press and a release at a point of the viewport in px and a frame, and then the
// frames that let the kit's transitions end. --input <file> gives the input as a file of actions instead, one to a
// line:
//
//   frame [count]               runs frames, one unless a count is given, a sixtieth of a second apart
//   settle                      runs the frames that let the kit's transitions end
//   move <x> <y>                queues the pointer's move to a point
//   down <x> <y> [press]...     queues a press of the primary button, or its release with `up`
//   click <x> <y> [press]...    queues a move, a press and a release, and runs a frame
//   secondary <x> <y>           queues a move, and a press and a release of the secondary button, and runs a frame
//   scroll <x> <y> <dx> <dy> [modifier]...
//                               queues a scroll of px at a point, with shift, control, alt or super held
//   key <name> [modifier]...    queues a key's press and release, by its name in UI Events, with shift, control, alt or
//                               super held
//   text <characters>           queues typed text, the rest of the line
//   screenshot <name> [<x> <y> <width> <height>]
//                               writes what the last frame shows to <name>.png in the directory of --out, or only the
//                               part of it in a rectangle of the viewport, in px
//
// A line that is empty or begins with # is skipped. Queued input is routed by the frames that follow it. Each word
// after a press's point says how the press is made: a modifier that is held, `secondary` for the secondary button in
// place of the primary, or `double` for the second press of a double click.

#include "pr/app_input.h"
#include "pr/array.h"
#include "pr/ascii.h"
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
#include "pr/string.h"
#include "pr/studio_app.h"
#include "pr/text_buffer.h"
#include "pr/text_harfbuzz.h"
#include "pr/time.h"
#include "pr/ui.h"
#include "pr/ui_records.h"

#include "examples/studio_gallery/gallery_state.h"
#include "examples/studio_gallery/gallery_view.h"
#include "examples/studio_gallery/section_components.h"
#include "examples/studio_gallery/section_controls.h"
#include "examples/studio_gallery/section_data.h"
#include "examples/studio_gallery/section_feedback.h"
#include "examples/studio_gallery/section_foundations.h"
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

// Runs the bundle at `path` in a new realm, after `prelude`, which defines what the host gives the script. Returns
// false, saying why on stderr, if the file cannot be read or a script throws.
bool scriptStart(ScriptHost& script, const char* path, PrStringView prelude) {
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
        ran = prJsHtmlEngineRun(ctx, prelude, "host", 1, 1) &&
            prJsHtmlEngineRun(ctx, PrStringView{source.data, source.size}, PrStringView(path), 1, 1);
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

// Tells the script of the round, if it follows the rounds, and declares its widgets under `root`, with what it changed
// in answer. Then tells the script of the round's input. What the script changes in answer to that is declared by the
// next round, which the context runs after a round that routed a press or a release, or by the frame that this asks
// for.
void scriptBuild(ScriptHost& script, PrKit* kit, PrLayoutNode root) {
    if (prJsKitTreeDispatchRound(script.tree, kit))
        prJsHtmlEngineCheckpoint(script.engine);
    prJsKitTreeBuild(script.tree, kit, root);
    if (prJsKitTreeDispatch(script.tree))
        prJsHtmlEngineCheckpoint(script.engine);
    if (prJsKitTreeIsChanged(script.tree))
        prUiRequestFrame(kit->ui);
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

// A section of examples/studio_gallery by the name that --native gives it, and what declares it.
struct GallerySection {
    PrStringView name;
    void (*build)(PrKit* kit, PrLayoutNode parent, gallery::GalleryState& state);
};

// Declares a section that reads no state.
template <void (*Build)(PrKit* kit, PrLayoutNode parent)>
void stateless(PrKit* kit, PrLayoutNode parent, gallery::GalleryState&) {
    Build(kit, parent);
}

constexpr GallerySection kGallerySections[] = {
    {"principles", stateless<gallery::sectionPrinciples>},
    {"color", gallery::sectionColor},
    {"type", stateless<gallery::sectionType>},
    {"space", gallery::sectionSpace},
    {"icons", stateless<gallery::sectionIcons>},
    {"buttons", stateless<gallery::sectionButtons>},
    {"inputs", gallery::sectionInputs},
    {"toggles", gallery::sectionToggles},
    {"tabs", gallery::sectionTabs},
    {"menus", gallery::sectionMenus},
    {"tree", gallery::sectionTree},
    {"inspector", gallery::sectionInspector},
    {"toolbar", gallery::sectionToolbar},
    {"tables", gallery::sectionTables},
    {"tags", gallery::sectionTags},
    {"progress", gallery::sectionProgress},
    {"tooltips", stateless<gallery::sectionTooltips>},
    {"dialogs", gallery::sectionDialogs},
    {"status", stateless<gallery::sectionStatus>},
    {"collections", gallery::sectionCollections},
    {"timeline", gallery::sectionTimeline},
    {"images", gallery::sectionImages},
    {"graph", gallery::sectionGraph},
    {"keys", gallery::sectionKeys},
};

// Declares one section alone on the gallery's page: the scrolling column of the gallery's content, and the box that the
// gallery gives each section (gallery_view.cpp), with no sidebar beside it.
void gallerySectionBuild(PrKit* kit, PrLayoutNode root, const GallerySection& section, gallery::GalleryState& state) {
    PrKitBoxStyle scroll{
        .overflow = PrKitOverflow_ScrollY,
        .gap = kPrKitSpace16,
        .padding = {kPrKitSpace10 * 2, kPrKitSpace12, kPrKitSpace16 * 3, kPrKitSpace12},
    };
    scroll.place.flex = PrKitFlex_Fill;
    PrLayoutNode content = prKitBox(kit, root, {}, scroll);
    section.build(kit, prKitBox(kit, content, {}, {.gap = kPrKitSpace3}), state);
}

// ---------------------------------------------------------------------------------------------------------------------
// What a run declares

// The program that --native names.
enum {
    Native_None,                // a bundle's script
    Native_HelloStyle,
    Native_Gallery,
    Native_GallerySection,
};
typedef uint8_t Native;

// A run's content: a bundle's script, or a native program and its model.
struct Content {
    ScriptHost script;
    Native native = Native_None;
    uint32_t clicks = 0;                                // the clicks that hello_style counts
    const GallerySection* PR_NULLABLE section = nullptr;
    gallery::GalleryState gallery;
};

void contentBuild(Content& content, PrKit* kit, PrLayoutNode root) {
    switch (content.native) {
        case Native_HelloStyle:
            helloStyleBuild(kit, root, content.clicks);
            break;
        case Native_Gallery:
            gallery::buildGallery(kit, root, content.gallery);
            break;
        case Native_GallerySection:
            gallerySectionBuild(kit, root, *content.section, content.gallery);
            break;
        default:
            scriptBuild(content.script, kit, root);
            break;
    }
}

// Reads the program that --native names into the content. Returns false if there is none of that name.
bool nativeFrom(PrStringView name, Content& content) {
    constexpr PrStringView kSectionPrefix = "gallery:";
    if (name == "hello_style") {
        content.native = Native_HelloStyle;
    } else if (name == "gallery") {
        content.native = Native_Gallery;
    } else if (name.size > kSectionPrefix.size && PrStringView{name.data, kSectionPrefix.size} == kSectionPrefix) {
        PrStringView sectionName{name.data + kSectionPrefix.size, name.size - kSectionPrefix.size};
        for (const GallerySection& section : kGallerySections) {
            if (section.name == sectionName) {
                content.native = Native_GallerySection;
                content.section = &section;
            }
        }
    }
    return content.native != Native_None;
}

// ---------------------------------------------------------------------------------------------------------------------
// Headless runs

// The number of frames that a headless run gives the kit's transitions to end, a sixtieth of a second apart.
constexpr uint32_t kSettleFrames = 60;

// The most times that a headless frame is declared again because the script asked for it in answer to the frame.
constexpr uint32_t kAnswerFrames = 2;

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

// Installs the fonts that Studio's applications set text in. Returns null, saying why on stderr, if they are missing.
PrFontSet* PR_NULLABLE fontsCreate() {
    PrFontSet* fonts = prFontSetCreate(nullptr);
    const PrFontBundle bundles[] = {PrFontBundle_Complete};
    if (prFontBundleInstall(fonts, bundles, uint32_t(countOf(bundles)), PR_FONT_BUNDLE_DIR) == 0) {
        printError("svelte_kit_host: the fonts under {} could not be installed\n", PR_FONT_BUNDLE_DIR);
        prFontSetDestroy(fonts);
        return nullptr;
    }
    return fonts;
}

bool headlessCreate(Headless& headless, PrUVec2 extent) {
    PrFontSet* fonts = fontsCreate();
    if (fonts == nullptr)
        return false;
    headless.fonts = fonts;
    headless.extent = extent;
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
    if (content.native == Native_None) {
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

    // What the script changed in answer to the frame is declared before the frame is shown, if the script asked for
    // that, as a program that declares its own widgets shows a value in the widgets that it declares after the one that
    // changed it. The frame's time stands still for it, so nothing that moves with time moves twice.
    desc.delta = 0;
    for (uint32_t again = 0; again < kAnswerFrames && content.native == Native_None &&
            prJsKitTreeTakeRoundRequest(content.script.tree); ++again)
        prUiFrame(headless.ui, &desc, PR_SCOPED_CALLBACK(round));
}

void headlessPush(Headless& headless, PrAppEvent event) {
    event.time = headless.time;
    prUiPushEvent(headless.ui, kPrUiViewportMain, &event);
}

// How a press is made: its button, its place in a multi-click, and the modifiers that are held.
struct Press {
    uint8_t button = 0;
    uint32_t clicks = 1;
    PrAppModifierFlags modifiers = 0;
};

// Queues an event of the pointer at a point: a move, or a press or its release made as `press` says.
void headlessPoint(Headless& headless, PrAppEventKind kind, PrVec2 at, const Press& press) {
    PrAppPointerEvent pointer{.position = at};
    if (kind != PrAppEventKind_PointerMove)
        pointer.button = press.button;
    if (kind == PrAppEventKind_PointerDown)
        pointer.clicks = press.clicks;
    headlessPush(headless, {.kind = kind, .modifiers = press.modifiers, .pointer = pointer});
}

void headlessClick(Headless& headless, PrVec2 at, const Press& press = {}) {
    headlessPoint(headless, PrAppEventKind_PointerMove, at, press);
    headlessPoint(headless, PrAppEventKind_PointerDown, at, press);
    headlessPoint(headless, PrAppEventKind_PointerUp, at, press);
}

// Renders the tree as the last frame laid it out, on the CPU, over the color that a window would clear to, and writes
// it to `path` as a PNG: all of it, or the part of it in `crop`, a rectangle of the viewport in target pixels, if one
// is given. Returns false if it cannot, or if the rectangle is empty or reaches past the viewport.
bool headlessScreenshot(Headless& headless, const char* path, const PrIRect* PR_NULLABLE crop = nullptr) {
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
        PrPixelView shown{rgba.data(), stride, headless.extent};
        bool inside = true;
        if (crop != nullptr) {
            inside = crop->left >= 0 && crop->top >= 0 && crop->left < crop->right && crop->top < crop->bottom &&
                uint32_t(crop->right) <= headless.extent.x && uint32_t(crop->bottom) <= headless.extent.y;
            if (inside) {
                shown = PrPixelView{
                    rgba.data() + size_t(crop->top) * stride + size_t(crop->left) * 4, stride,
                    {uint32_t(crop->right - crop->left), uint32_t(crop->bottom - crop->top)},
                };
            }
        }
        written = inside && prDrawRendererRenderPixels(renderer, list, pixels, PrPixelFormat_Rgba8) == PrStatus_Ok &&
            harnessWritePng(path, shown);
        prDrawListRelease(list);
        prDrawRendererDestroy(renderer);
    }
    prDrawGlyphDecoderDestroy(glyphs);
    prImageCodecDestroy(images);
    prCompressCodecDestroy(compress);
    return written;
}

// ---------------------------------------------------------------------------------------------------------------------
// Input files

// Takes the next word of `line`, which spaces end, and leaves `line` after it. Returns an empty word at the line's end.
PrStringView takeWord(PrStringView& line) {
    size_t start = 0;
    while (start < line.size && line.data[start] == ' ')
        ++start;
    size_t end = start;
    while (end < line.size && line.data[end] != ' ')
        ++end;
    PrStringView word{line.data + start, end - start};
    line = PrStringView{line.data + end, line.size - end};
    return word;
}

// Takes the next word of `line` as a decimal number, with an optional sign and fraction. Returns false if the next word
// is not one.
bool takeNumber(PrStringView& line, float* out) {
    PrStringView word = takeWord(line);
    size_t at = 0;
    bool negative = at < word.size && word.data[at] == '-';
    if (negative)
        ++at;
    double value = 0;
    double divisor = 1;
    bool digits = false;
    bool fraction = false;
    for (; at < word.size; ++at) {
        char c = word.data[at];
        if (c == '.' && !fraction) {
            fraction = true;
        } else if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            if (fraction)
                divisor *= 10;
            digits = true;
        } else {
            return false;
        }
    }
    *out = float((negative ? -value : value) / divisor);
    return digits;
}

bool takePoint(PrStringView& line, PrVec2* out) {
    return takeNumber(line, &out->x) && takeNumber(line, &out->y);
}

// Returns the named key that has the name of UI Events `name`, or else the printable key of its single ASCII character,
// or PrAppKey_Unknown.
PrAppKey keyFrom(PrStringView name) {
    for (uint32_t key = PrAppKey_Left; key <= PrAppKey_F20; ++key) {
        if (prAppKeyName(PrAppKey(key)) == name)
            return PrAppKey(key);
    }
    if (name.size == 1 && uint8_t(name.data[0]) < 128)
        return PrAppKey(prAsciiUpper(name.data[0]));
    return PrAppKey_Unknown;
}

// Reads the modifiers that the rest of `line` names. Returns false for a word that names none.
bool takeModifiers(PrStringView& line, PrAppModifierFlags* out) {
    PrAppModifierFlags modifiers = 0;
    for (PrStringView word = takeWord(line); word.size > 0; word = takeWord(line)) {
        if (word == "shift")
            modifiers |= PrAppModifierFlag_Shift;
        else if (word == "control")
            modifiers |= PrAppModifierFlag_Control;
        else if (word == "alt")
            modifiers |= PrAppModifierFlag_Alt;
        else if (word == "super")
            modifiers |= PrAppModifierFlag_Super;
        else
            return false;
    }
    *out = modifiers;
    return true;
}

// Reads how the rest of `line` says a press is made: `secondary`, `double`, and the modifiers that it names. Returns
// false for any other word.
bool takePress(PrStringView& line, Press* out) {
    Press press;
    for (PrStringView word = takeWord(line); word.size > 0; word = takeWord(line)) {
        PrAppModifierFlags modifier = 0;
        if (word == "secondary")
            press.button = 1;
        else if (word == "double")
            press.clicks = 2;
        else if (!takeModifiers(word, &modifier))
            return false;
        press.modifiers |= modifier;
    }
    *out = press;
    return true;
}

// Performs one action of an input file. Returns false if the line is not an action, or its screenshot is not written.
bool headlessAct(Headless& headless, Content& content, PrStringView line, PrStringView out) {
    PrStringView action = takeWord(line);
    PrVec2 at{};
    if (action.size == 0 || action.data[0] == '#')
        return true;
    if (action == "frame") {
        float count = 1;
        PrStringView rest = line;
        if (takeWord(rest).size > 0 && !takeNumber(line, &count))
            return false;
        for (uint32_t i = 0; i < uint32_t(count); ++i)
            headlessFrame(headless, content);
        return true;
    }
    if (action == "settle") {
        for (uint32_t i = 0; i < kSettleFrames; ++i)
            headlessFrame(headless, content);
        return true;
    }
    if (action == "move" && takePoint(line, &at)) {
        headlessPush(headless, {.kind = PrAppEventKind_PointerMove, .pointer = {.position = at}});
        return true;
    }
    Press press;
    if (action == "down" && takePoint(line, &at) && takePress(line, &press)) {
        headlessPoint(headless, PrAppEventKind_PointerDown, at, press);
        return true;
    }
    if (action == "up" && takePoint(line, &at) && takePress(line, &press)) {
        headlessPoint(headless, PrAppEventKind_PointerUp, at, press);
        return true;
    }
    if (action == "click" && takePoint(line, &at) && takePress(line, &press)) {
        headlessClick(headless, at, press);
        headlessFrame(headless, content);
        return true;
    }
    if (action == "secondary" && takePoint(line, &at)) {
        // The secondary button is button 1 (pointerevents3/#the-button-property).
        headlessClick(headless, at, Press{.button = 1});
        headlessFrame(headless, content);
        return true;
    }
    PrVec2 delta{};
    PrAppModifierFlags held = 0;
    if (action == "scroll" && takePoint(line, &at) && takePoint(line, &delta) && takeModifiers(line, &held)) {
        headlessPush(headless, {
            .kind = PrAppEventKind_Scroll,
            .modifiers = held,
            .scroll = {.position = at, .delta = delta},
        });
        return true;
    }
    if (action == "key") {
        PrAppKey key = keyFrom(takeWord(line));
        PrAppModifierFlags modifiers = 0;
        if (key == PrAppKey_Unknown || !takeModifiers(line, &modifiers))
            return false;
        headlessPush(headless, {.kind = PrAppEventKind_KeyDown, .modifiers = modifiers, .keyboard = {.key = key}});
        headlessPush(headless, {.kind = PrAppEventKind_KeyUp, .modifiers = modifiers, .keyboard = {.key = key}});
        return true;
    }
    if (action == "text") {
        while (line.size > 0 && line.data[0] == ' ')
            line = PrStringView{line.data + 1, line.size - 1};
        headlessPush(headless, {.kind = PrAppEventKind_Text, .input = {.text = line}});
        return true;
    }
    if (action == "screenshot") {
        PrStringView name = takeWord(line);
        if (name.size == 0)
            return false;
        String path;
        path.append(out);
        path.append("/");
        path.append(name);
        path.append(".png");
        path.append(PrStringView{"\0", 1});
        PrStringView rest = line;
        if (takeWord(rest).size == 0)
            return headlessScreenshot(headless, path.data());
        PrVec2 origin{};
        PrVec2 size{};
        if (!takePoint(line, &origin) || !takePoint(line, &size) || origin.x < 0 || origin.y < 0 || size.x <= 0 ||
                size.y <= 0)
            return false;
        PrIRect crop{int32_t(origin.x), int32_t(origin.y), int32_t(origin.x + size.x), int32_t(origin.y + size.y)};
        return headlessScreenshot(headless, path.data(), &crop);
    }
    return false;
}

// Performs the actions of the input file at `path`, a line at a time. Returns false, saying which line on stderr, at
// the first that fails.
bool headlessInput(Headless& headless, Content& content, const char* path, PrStringView out) {
    PrTextBuffer file{};
    if (!harnessReadFile(&file, path)) {
        printError("svelte_kit_host: {} cannot be read\n", PrStringView(path));
        return false;
    }
    bool ok = true;
    uint32_t number = 0;
    for (uint32_t start = 0; ok && start < file.size;) {
        uint32_t end = start;
        while (end < file.size && file.data[end] != '\n')
            ++end;
        uint32_t size = end - start;
        if (size > 0 && file.data[end - 1] == '\r')
            --size;
        ++number;
        ok = headlessAct(headless, content, PrStringView{file.data + start, size}, out);
        if (!ok)
            printError("svelte_kit_host: {}:{}: not an action that could be done\n", PrStringView(path), number);
        start = end + 1;
    }
    prTextBufferReset(&file);
    return ok;
}

// What a headless run is asked for beyond its content.
struct HeadlessRun {
    PrUVec2 extent = {720, 480};
    Array<PrVec2> clicks;
    const char* PR_NULLABLE input = nullptr;
    const char* PR_NULLABLE screenshot = nullptr;
    PrStringView out = ".";
};

int runHeadless(Content& content, const HeadlessRun& run) {
    Headless headless{};
    if (!headlessCreate(headless, run.extent))
        return 1;
    // The first frame of a context routes no input, and lays out what the input then hits.
    headlessFrame(headless, content);
    int status = 0;
    if (run.input != nullptr) {
        if (!headlessInput(headless, content, run.input, run.out))
            status = 1;
    } else {
        for (PrVec2 at : run.clicks) {
            headlessClick(headless, at);
            headlessFrame(headless, content);
        }
        for (uint32_t i = 0; i < kSettleFrames; ++i)
            headlessFrame(headless, content);
    }
    if (run.screenshot != nullptr && !headlessScreenshot(headless, run.screenshot)) {
        printError("svelte_kit_host: {} could not be written\n", PrStringView(run.screenshot));
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
    if (windowed->content->native == Native_None)
        scriptPump(windowed->content->script);
}

void windowBuild(void* context, PrStudioWindow* window, PrLayoutNode root, PrVec2) {
    contentBuild(*static_cast<Windowed*>(context)->content, prStudioWindowKit(window), root);
}

// Asks for the frame that the script's next task is due in, so that a timer runs with no input to wake the window.
bool windowLaidOut(void* context, PrStudioWindow* window) {
    Windowed* windowed = static_cast<Windowed*>(context);
    if (windowed->content->native != Native_None)
        return false;
    double delay = prDomAgentNextTaskDelay(prHtmlPageAgent(windowed->content->script.page));
    if (delay == 0)
        return true;
    if (delay > 0)
        prUiRequestFrameAt(prStudioWindowUi(window), windowed->time + delay / 1000);
    return false;
}

int runWindow(Content& content, PrUVec2 extent) {
    PrFontSet* fonts = fontsCreate();
    if (fonts == nullptr)
        return 1;
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

// Appends `text` to `out` as a string literal of JavaScript.
void appendLiteral(String& out, PrStringView text) {
    out.append("\"");
    for (size_t i = 0; i < text.size; ++i) {
        char c = text.data[i];
        if (c == '"' || c == '\\')
            out.append("\\");
        if (c == '\n')
            out.append("\\n");
        else
            out.append(PrStringView{&c, 1});
    }
    out.append("\"");
}

// Appends the property that --arg's `argument` gives, `name=value`, to the script that defines `hostArguments`. Returns
// false if the argument has no name.
bool appendArgument(String& prelude, PrStringView argument) {
    size_t equals = 0;
    while (equals < argument.size && argument.data[equals] != '=')
        ++equals;
    if (equals == 0 || equals == argument.size)
        return false;
    prelude.append("hostArguments[");
    appendLiteral(prelude, PrStringView{argument.data, equals});
    prelude.append("] = ");
    appendLiteral(prelude, PrStringView{argument.data + equals + 1, argument.size - equals - 1});
    prelude.append(";\n");
    return true;
}

int usage() {
    printError("usage: svelte_kit_host [--headless [--click <x>,<y>]... [--input <file>] [--out <directory>] "
        "[--screenshot <png>]] [--extent <width>x<height>] [--arg <name>=<value>]... "
        "<bundle.js | --native <hello_style | gallery | gallery:<section>>>\n");
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    prCrashHandler();
    bool headless = false;
    HeadlessRun run;
    const char* bundle = nullptr;
    static Content content;
    String prelude;
    prelude.append("globalThis.hostArguments = {};\n");
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
            run.clicks.push(PrVec2{float(x), float(y)});
        } else if (argument == "--extent" && hasValue) {
            if (!readPair(argv[++i], 'x', &run.extent.x, &run.extent.y) || run.extent.x == 0 || run.extent.y == 0)
                return usage();
        } else if (argument == "--input" && hasValue) {
            run.input = argv[++i];
        } else if (argument == "--out" && hasValue) {
            run.out = argv[++i];
        } else if (argument == "--screenshot" && hasValue) {
            run.screenshot = argv[++i];
        } else if (argument == "--arg" && hasValue) {
            if (!appendArgument(prelude, argv[++i]))
                return usage();
        } else if (argument == "--native" && hasValue) {
            if (!nativeFrom(argv[++i], content))
                return usage();
        } else if (argument.size > 0 && argument.data[0] != '-' && bundle == nullptr) {
            bundle = argv[i];
        } else {
            return usage();
        }
    }
    bool scripted = content.native == Native_None;
    bool asksHeadless = run.clicks.count() > 0 || run.input != nullptr || run.screenshot != nullptr;
    if (scripted != (bundle != nullptr) || (!headless && asksHeadless) || (run.input != nullptr && run.clicks.count() > 0))
        return usage();
    content.script.virtualClock = headless;
    if (scripted && !scriptStart(content.script, bundle, PrStringView{prelude.data(), prelude.count()})) {
        scriptStop(content.script);
        return 1;
    }
    int status = headless ? runHeadless(content, run) : runWindow(content, run.extent);
    if (scripted)
        scriptStop(content.script);
    return status;
}
