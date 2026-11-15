// The kinds of Studio Kit's images in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/base64.h"
#include "pr/color.h"
#include "pr/draw.h"
#include "pr/draw_image.h"
#include "pr/kit_box.h"
#include "pr/kit_image.h"
#include "pr/kit_text.h"
#include "pr/pixels.h"
#include "pr/status.h"

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

// A keyword of an attribute, and the value of the kit's that it stands for.
struct Keyword {
    const char* name;
    uint8_t value;
};

// A picture of `width` by `height` pixels: a ramp of red across it and of green down it, in squares of `cell` pixels
// that are alternately opaque and half transparent, so that no fit or alignment of it looks like another.
Array<uint8_t> pictureOf(uint32_t width, uint32_t height, uint32_t cell) {
    Array<uint8_t> rgba(width * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t* pixel = &rgba[(y * width + x) * 4];
            pixel[0] = uint8_t(x * 255 / width);
            pixel[1] = uint8_t(y * 255 / height);
            pixel[2] = 160;
            pixel[3] = (x / cell + y / cell) % 2 == 0 ? 255 : 128;
        }
    }
    return rgba;
}

// Returns an image of `rgba`, which the caller releases.
const PrDrawImage* imageOf(uint32_t width, uint32_t height, const Array<uint8_t>& rgba) {
    PrDrawImageDesc desc{
        .pixels = PrPixelView{rgba.data(), size_t(width) * 4, {width, height}},
        .format = PrPixelFormat_Rgba8,
        .alpha = PrDrawAlpha_Straight,
        .space = PrColorSpace_Srgb,
    };
    const PrDrawImage* image = nullptr;
    REQUIRE(prDrawImageCreate(&desc, &image) == PrStatus_Ok);
    return image;
}

// Defines the script's global `name` as the base64 of `bytes`.
void defineBase64(JsKitScripted& scripted, const char* name, const Array<uint8_t>& bytes) {
    PrStringView input{reinterpret_cast<const char*>(bytes.data()), bytes.count()};
    Array<char> encoded(prBase64EncodedSize(input.size));
    prBase64Encode(input, encoded.data(), encoded.count());
    JSContext* ctx = scripted.ctx;
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, name, JS_NewStringLen(ctx, encoded.data(), encoded.count()));
    JS_FreeValue(ctx, global);
}

// Sets the attribute `name` of the script's node `node`, a global of the script, to `value`.
void set(JsKitScripted& scripted, const char* node, const char* name, const char* value) {
    String source("if (!kitTree.set(");
    source.append(node).append(", '").append(name).append("', '").append(value);
    source.append("')) throw new Error('refused');").append('\0');
    scripted.run(source.data());
}

} // namespace

} // namespace pr_impl

using namespace pr_impl;

TEST_CASE("an image shows the pixels that a script gives it, fitted and aligned as its attributes say") {
    JsKitScripted scripted;
    Array<uint8_t> ramp = pictureOf(64, 32, 8);
    Array<uint8_t> tiles = pictureOf(16, 24, 4);
    defineBase64(scripted, "ramp", ramp);
    defineBase64(scripted, "tiles", tiles);
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'start', gap: '8', padding: '8'});
        var natural = make('image', row, {pixels: '64 32 ' + ramp});
        var framed = make('image', row, {
            pixels: ' 64  32  ' + ramp, fit: 'cover', 'align-x': 'start', 'align-y': 'end', width: '40', height: '40',
            flex: 'fixed', margin: '0 4',
        });
        make('image', row, {fit: 'contain', width: '40', height: '40'});
        make('label', natural, {}, 'An image holds nothing, so this is not declared');
        make('label', row, {}, 'after the images');
    )JS");
    KitFixture direct;
    const PrDrawImage* wide = imageOf(64, 32, ramp);
    const PrDrawImage* tall = imageOf(16, 24, tiles);
    const PrDrawImage* first = wide;
    PrKitImageStyle frame{
        .fit = PrKitImageFit_Cover,
        .alignX = PrKitAlign_Start,
        .alignY = PrKitAlign_End,
        .place = {.width = prKitPx(40), .height = prKitPx(40), .margin = prKitEdgesXY(0, 4), .flex = PrKitFlex_Fixed},
    };
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Start,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        });
        if (first != nullptr)
            prKitImage(kit, row, {.image = first}, {});
        prKitImage(kit, row, {.image = wide}, frame);
        prKitLabel(kit, row, {.text = "after the images"}, {});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    const Keyword fits[] = {
        {"fill", PrKitImageFit_Fill}, {"contain", PrKitImageFit_Contain}, {"cover", PrKitImageFit_Cover},
        {"none", PrKitImageFit_None}, {"scale-down", PrKitImageFit_ScaleDown},
    };
    for (const Keyword& keyword : fits) {
        CAPTURE(keyword.name);
        set(scripted, "framed", "fit", keyword.name);
        frame.fit = keyword.value;
        frames(scripted, direct);
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }
    // The picture's own size leaves room in the box's height and is cropped in its width, so each alignment shows.
    set(scripted, "framed", "fit", "none");
    frame.fit = PrKitImageFit_None;
    const Keyword alignments[] = {
        {"auto", PrKitAlign_Auto}, {"start", PrKitAlign_Start}, {"center", PrKitAlign_Center}, {"end", PrKitAlign_End},
    };
    for (const Keyword& keyword : alignments) {
        CAPTURE(keyword.name);
        set(scripted, "framed", "align-x", keyword.name);
        set(scripted, "framed", "align-y", keyword.name);
        frame.alignX = keyword.value;
        frame.alignY = keyword.value;
        frames(scripted, direct);
        CHECK(jsKitRendersAlike(scripted.fixture, direct));
    }

    // Pixels that are not as many as their width and height say are refused, and the image keeps its picture.
    scripted.run("var refused = kitTree.set(natural, 'pixels', '64 31 ' + ramp);");
    CHECK(scripted.number("refused ? 1 : 0") == 0);
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // The image shows another picture once the script gives one, and nothing once its pixels are reset.
    scripted.run("kitTree.set(natural, 'pixels', '16 24 ' + tiles);");
    first = tall;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.run("kitTree.set(natural, 'pixels', null);");
    first = nullptr;
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
    prDrawImageRelease(wide);
    prDrawImageRelease(tall);
}

TEST_CASE("an image takes only pixels that are whole, and only its own attributes") {
    JsKitScripted scripted;
    Array<uint8_t> dot = pictureOf(2, 2, 1);
    defineBase64(scripted, "dot", dot);
    scripted.run(R"JS(
        var image = kitTree.create('image');
        var taken = [
            kitTree.set(image, 'pixels', '2 2 ' + dot), kitTree.set(image, 'pixels', '1 4 ' + dot),
            kitTree.set(image, 'pixels', '4 1 ' + dot.replace(/=+$/, '')),
            kitTree.set(image, 'pixels', '2 2 ' + dot.slice(0, 8) + ' ' + dot.slice(8)),
            kitTree.set(image, 'pixels', null), kitTree.set(image, 'fit', 'scale-down'),
            kitTree.set(image, 'align-x', 'end'), kitTree.set(image, 'align-y', null),
            kitTree.set(image, 'max-width', '100%'),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(image, 'pixels', ''), kitTree.set(image, 'pixels', '2 2'), kitTree.set(image, 'pixels', dot),
            kitTree.set(image, 'pixels', '2 ' + dot), kitTree.set(image, 'pixels', '0 2 ' + dot),
            kitTree.set(image, 'pixels', '2 0 ' + dot), kitTree.set(image, 'pixels', '2 3 ' + dot),
            kitTree.set(image, 'pixels', '2 1 ' + dot), kitTree.set(image, 'pixels', '2.5 2 ' + dot),
            kitTree.set(image, 'pixels', '-2 -2 ' + dot), kitTree.set(image, 'pixels', '2 2 ' + dot.replace('A', '!')),
            kitTree.set(image, 'pixels', '4294967295 4294967295 ' + dot),
            kitTree.set(image, 'pixels', '65536 65536 ' + dot), kitTree.set(image, 'pixels', '2 2 2 ' + dot),
            kitTree.set(image, 'fit', 'stretch'), kitTree.set(image, 'fit', ''),
            kitTree.set(image, 'align-x', 'baseline'), kitTree.set(image, 'align-y', 'stretch'),
            kitTree.set(image, 'src', 'picture.png'), kitTree.set(image, 'align', 'center'),
            kitTree.listen(image, 'load', true), kitTree.listen(image, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 9);
    CHECK(scripted.number("refused") == 0);
}
