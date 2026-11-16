// The kinds of Studio Kit's color controls (pr/kit_color.h): `color-field` and `color-picker`.
//
// A script gives a color in one of the two forms that jsKitSetColor reads. The first is the hex value, `#RRGGBB` or
// `#RRGGBBAA`. The second is three or four numbers separated by spaces: the red, green and blue channels of extended
// sRGB, and the alpha, which is 1 if it is left out. The numbers give a color that the hex value cannot: one past sRGB,
// whose channels lie outside 0 to 1, or one finer than 8 bits to a channel, as a drag on a picker's plane makes.
//
// A control takes its color as its `value`, and queues `change` in each round in which the user edits the color. The
// event gives the color in both forms: the four channels as `values`, exactly, and the hex value as `text`
// (prKitColorHex), which rounds each channel to 8 bits within sRGB. The node keeps the edit, so a control works with no
// listener. A script that sets `value` to the channels of an event changes nothing. Neither does a script that sets it
// to the hex value of an event: a hex value that the node's color already rounds to leaves the color as it is, so that
// the script does not round a color under the drag that makes it. A script that sets another color replaces the edit.
//
// color-field (prKitColorField)
//   Attributes
//     value          a color
//     disabled       a flag
//     size           a control size
//   Events
//     change         an edit in the field's picker, with the new color
//   The field opens its picker in a popover as it is activated, and the kit keeps whether the picker is open.
//
// color-picker (prKitColorPicker)
//   Attributes
//     value          a color
//     mode           the plane that the picker opens on, a keyword of kModes, such as `oklch`. The kit keeps the plane
//                    that the picker's tabs choose, so a later `mode` changes nothing.
//     gamut          the RGB space that the perceptual planes work in, a keyword of kGamuts, such as `display-p3`, or
//                    `display` for the gamut of the display that the round is shown on
//     opaque         a flag
//   Events
//     change         an edit, with the new color

#include "studio/kit/quickjs/src/kit_color_quickjs.h"

#include "pr/kit_color.h"

namespace pr_impl {

namespace {

// The events of a color control, by their index.
enum { ColorEvent_Change };
constexpr PrStringView kColorEvents[] = {"change"};

// Sets a control's color. A hex value that the color already rounds to leaves the color as it is, so that a script
// which gives a control the hex value of its own edit does not round the edit.
bool colorSet(PrColor& color, PrStringView value, bool reset) {
    PrColor given = {};
    if (!jsKitSetColor(value, reset, &given))
        return false;
    PrColor parsed = {};
    bool hex = !reset && prKitColorParseHex(value, &parsed);
    char held[9];
    char taken[9];
    if (!hex || prKitColorHex(color, held) != prKitColorHex(given, taken))
        color = given;
    return true;
}

// Queues a change with `color`, if the kit's control edited it since `before` was copied from it.
void colorReport(const JsKitRound& round, JsKitNodeIndex node, PrColor color, PrColor before) {
    if (color.r == before.r && color.g == before.g && color.b == before.b && color.a == before.a)
        return;
    char hex[9];
    JsKitDetail detail{
        .values = {double(color.r), double(color.g), double(color.b), double(color.a)},
        .valueCount = 4,
        .hasText = true,
        .text = prKitColorHex(color, hex),
    };
    jsKitEventPush(round, node, ColorEvent_Change, &detail);
}

struct FieldData {
    PrColor color;
    PrKitColorFieldStyle style;
    bool disabled;
};

bool fieldSet(void* data, PrStringView name, PrStringView value, bool reset) {
    FieldData& field = *static_cast<FieldData*>(data);
    if (name == "value")
        return colorSet(field.color, value, reset);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &field.disabled);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &field.style.size);
    return false;
}

PrLayoutNode fieldDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    FieldData& field = jsKitData<FieldData>(round, node);
    PrColor before = field.color;
    PrKitColorField content{.key = jsKitNodeKey(round, node), .color = &field.color, .disabled = field.disabled};
    PrLayoutNode made = prKitColorField(round.kit, parent, content, field.style);
    colorReport(round, node, field.color, before);
    return made;
}

constexpr JsKitKeyword kModes[] = {
    {"hsv", PrKitColorMode_Hsv},
    {"oklch", PrKitColorMode_Oklch},
    {"ramp", PrKitColorMode_Ramp},
};

// What the keyword `display` stands for among a picker's gamuts: the gamut of the display that the round is shown on,
// which the picker reads as it is declared.
constexpr uint8_t kGamutDisplay = PrColorSpace_Count;

// The gamuts that a picker takes: the RGB spaces of standard dynamic range, and the display's own.
constexpr JsKitKeyword kGamuts[] = {
    {"srgb", PrColorSpace_Srgb},
    {"srgb-linear", PrColorSpace_SrgbLinear},
    {"display-p3", PrColorSpace_DisplayP3},
    {"display-p3-linear", PrColorSpace_DisplayP3Linear},
    {"a98-rgb", PrColorSpace_A98Rgb},
    {"pro-photo-rgb", PrColorSpace_ProPhotoRgb},
    {"rec2020", PrColorSpace_Rec2020},
    {"display", kGamutDisplay},
};

struct PickerData {
    PrColor color;
    PrKitColorMode mode;
    uint8_t gamut;                  // a PrColorSpace, or kGamutDisplay
    bool opaque;
};

bool pickerSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PickerData& picker = *static_cast<PickerData*>(data);
    if (name == "value")
        return colorSet(picker.color, value, reset);
    if (name == "mode")
        return jsKitSetKeyword(kModes, value, reset, &picker.mode);
    if (name == "gamut")
        return jsKitSetKeyword(kGamuts, value, reset, &picker.gamut);
    if (name == "opaque")
        return jsKitSetFlag(value, reset, &picker.opaque);
    return false;
}

PrLayoutNode pickerDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PickerData& picker = jsKitData<PickerData>(round, node);
    PrColor before = picker.color;
    PrKitColorPicker content{
        .key = jsKitNodeKey(round, node),
        .color = &picker.color,
        .mode = picker.mode,
        .gamut = picker.gamut == kGamutDisplay ? prUiDisplay(round.kit->ui).gamut : PrColorSpace(picker.gamut),
        .opaque = picker.opaque,
    };
    PrLayoutNode made = prKitColorPicker(round.kit, parent, content);
    colorReport(round, node, picker.color, before);
    return made;
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "color-field",
        .dataSize = sizeof(FieldData),
        .events = kColorEvents,
        .eventCount = 1,
        .set = fieldSet,
        .declare = fieldDeclare,
    },
    {
        .name = "color-picker",
        .dataSize = sizeof(PickerData),
        .events = kColorEvents,
        .eventCount = 1,
        .set = pickerSet,
        .declare = pickerDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitColorKinds() {
    return kKinds;
}

} // namespace pr_impl
