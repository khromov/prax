// The kinds of Studio Kit's images (pr/kit_image.h): `image`.
//
// An `image` (prKitImage) takes `pixels`, the picture that it shows, and the fields of PrKitImageStyle: `fit`, a
// keyword of PrKitImageFit, and `align-x` and `align-y`, each `auto`, `start`, `center` or `end`. Its style's place is
// the node's placement. It shows no text and queues no event, and the nodes under it are not declared, since an image
// holds nothing.
//
// `pixels` is the picture's width and its height in pixels, each a whole number above zero, and then the base64 of its
// pixels (RFC 4648), all separated by spaces. The pixels are rows from the top, of four bytes for each pixel: red,
// green, blue and alpha, in sRGB with straight alpha. There must be exactly as many pixels as the width and the height
// say. The node makes a picture of its own from them as the attribute is set, so a script sets the attribute when the
// picture changes, not in every round, and two nodes that show the same picture each hold a copy of it. An image whose
// `pixels` is reset, as it is at first, declares nothing and takes no room.
//
// A script gives pixels and nothing else: the binding decodes no encoded image, such as a PNG, and loads nothing from a
// file or a URL.

#include "studio/kit/quickjs/src/kit_image_quickjs.h"

#include "pr/base64.h"
#include "pr/color.h"
#include "pr/draw.h"
#include "pr/draw_image.h"
#include "pr/kit_image.h"
#include "pr/memory.h"
#include "pr/pixels.h"
#include "pr/status.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kFits[] = {
    {"fill", PrKitImageFit_Fill},
    {"contain", PrKitImageFit_Contain},
    {"cover", PrKitImageFit_Cover},
    {"none", PrKitImageFit_None},
    {"scale-down", PrKitImageFit_ScaleDown},
};

// Where an image sits in the room that its fit leaves or crops.
constexpr JsKitKeyword kImageAlignments[] = {
    {"auto", PrKitAlign_Auto},
    {"start", PrKitAlign_Start},
    {"center", PrKitAlign_Center},
    {"end", PrKitAlign_End},
};

// What an image is declared from. The style's place is the node's.
struct ImageData {
    const PrDrawImage* PR_NULLABLE image;   // the picture that `pixels` gave, which the node holds a reference to
    PrKitImageStyle style;
};

// Returns the next word of `text` at or after `*at`, and moves `*at` past it. Spaces separate words. Returns an empty
// view once no word is left.
PrStringView nextWord(PrStringView text, size_t* at) {
    while (*at < text.size && text.data[*at] == ' ')
        ++*at;
    size_t start = *at;
    while (*at < text.size && text.data[*at] != ' ')
        ++*at;
    return PrStringView{text.data + start, *at - start};
}

// Creates the image that the value of `pixels` describes, with a reference that the caller holds. Returns false, and
// writes nothing, if the text is not a width, a height and the base64 of that many pixels.
bool imageFrom(PrStringView text, const PrDrawImage* PR_NULLABLE* PR_NONNULL out) {
    size_t at = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    if (!jsKitSetUint(nextWord(text, &at), false, &width) || !jsKitSetUint(nextWord(text, &at), false, &height) ||
            width == 0 || height == 0)
        return false;
    // The base64 is the rest of the text, and bounds the pixels that it can hold, so a width and a height that claim
    // more are refused before anything is allocated for them.
    PrStringView encoded{text.data + at, text.size - at};
    size_t capacity = prBase64DecodedCapacity(encoded.size);
    uint64_t size = uint64_t(width) * height * 4;
    if (size > capacity)
        return false;
    char* bytes = static_cast<char*>(prMemoryAlloc(capacity));
    size_t decoded = 0;
    const PrDrawImage* image = nullptr;
    if (prBase64Decode(encoded, bytes, capacity, &decoded) && decoded == size) {
        PrDrawImageDesc desc{
            .pixels = PrPixelView{reinterpret_cast<const uint8_t*>(bytes), size_t(width) * 4, {width, height}},
            .format = PrPixelFormat_Rgba8,
            .alpha = PrDrawAlpha_Straight,
            .space = PrColorSpace_Srgb,
        };
        if (prDrawImageCreate(&desc, &image) != PrStatus_Ok)
            image = nullptr;
    }
    prMemoryFree(bytes);
    if (image == nullptr)
        return false;
    *out = image;
    return true;
}

void imageDestroy(void* data) {
    prDrawImageRelease(static_cast<ImageData*>(data)->image);
}

bool imageSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ImageData& image = *static_cast<ImageData*>(data);
    if (name == "pixels") {
        const PrDrawImage* made = nullptr;
        if (!reset && !imageFrom(value, &made))
            return false;
        prDrawImageRelease(image.image);
        image.image = made;
        return true;
    }
    if (name == "fit")
        return jsKitSetKeyword(kFits, value, reset, &image.style.fit);
    if (name == "align-x")
        return jsKitSetKeyword(kImageAlignments, value, reset, &image.style.alignX);
    if (name == "align-y")
        return jsKitSetKeyword(kImageAlignments, value, reset, &image.style.alignY);
    return false;
}

// An image is a replaced element, which holds no nodes, so its kind returns no node for the tree to declare the node's
// children under.
PrLayoutNode imageDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const ImageData& image = jsKitData<ImageData>(round, node);
    if (image.image == nullptr)
        return kPrLayoutNodeNone;
    PrKitImageStyle style = image.style;
    style.place = jsKitNodePlace(round.tree, node);
    prKitImage(round.kit, parent, {.key = jsKitNodeKey(round, node), .image = image.image}, style);
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[] = {
    {.name = "image", .dataSize = sizeof(ImageData), .destroy = imageDestroy, .set = imageSet, .declare = imageDeclare},
};

} // namespace

Span<const JsKitKind> jsKitImageKinds() {
    return kKinds;
}

} // namespace pr_impl
