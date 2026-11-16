// The kinds of Studio Kit's panels (pr/kit_panel.h): `panel`, `selectable`, `panel-header` and `swatch`.
//
// A `panel` (prKitPanel) is a container that takes the attributes of a `box`, and the fields of PrKitPanelStyle:
// `background`, a keyword of PrKitBackground, and `rules`, the sides that it rules, which are one or more of `top`,
// `right`, `bottom`, `left` and `all` separated by spaces. It shows no text and queues no event.
//
// A `selectable` (prKitSelectable) is a container that takes the attributes of a `box`, where `disabled` is the
// selectable's own, and `selected`, a flag. It shows no text. It queues `click`, with no detail, when it is activated.
// The script sets `selected` in answer, since the widget keeps no selection of its own.
//
// A `panel-header` (prKitPanelHeader) shows its node's text as its title, and takes `meta`, a string. The nodes under
// it are declared in its bar, after the meta. It queues no event.
//
// A `swatch` (prKitSwatch) takes `color`, a color, and `size`, the side of its square in px. It shows no text and
// queues no event. A color is the name of a color of the kit's palette, which follows the theme, or a color in sRGB
// with straight alpha, written either as `#` and three, four, six or eight hexadecimal digits or as three or four
// numbers separated by spaces: red, green, blue and alpha, where 1 is a channel's full value and alpha is 1 if it is
// left out. The names of the palette's colors are those of PrKitPalette's fields, with a hyphen between a name's words
// and none before a digit: `accent-weak`, `bg0`, `text-inverse`. A name may be followed by a space and a shade, a
// number from -1 to 1, which gives a tint or a shade of the palette's color that follows the theme as the color does. A
// shade above zero is the share of white that the color's red, green and blue are mixed with, and a shade below zero is
// the share of black, negated: `accent 0.6` is the accent with six tenths of white, and `accent -0.2` the accent with
// two tenths of black. The mix keeps the color's alpha. Reset, a color is transparent.

#include "studio/kit/quickjs/src/kit_panel_quickjs.h"

#include "pr/ascii.h"
#include "pr/kit_panel.h"

#include "studio/kit/quickjs/src/kit_box_quickjs.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kBackgrounds[] = {
    {"none", PrKitBackground_None},
    {"app", PrKitBackground_App},
    {"panel", PrKitBackground_Panel},
    {"header", PrKitBackground_Header},
    {"raised", PrKitBackground_Raised},
    {"select", PrKitBackground_Select},
    {"viewport", PrKitBackground_Viewport},
    {"hud", PrKitBackground_Hud},
};

constexpr JsKitKeyword kRules[] = {
    {"top", PrKitRuleFlag_Top},
    {"right", PrKitRuleFlag_Right},
    {"bottom", PrKitRuleFlag_Bottom},
    {"left", PrKitRuleFlag_Left},
    {"all", PrKitRuleFlag_All},
};

// The events of a selectable, by their index.
enum { SelectableEvent_Click };
constexpr PrStringView kSelectableEvents[] = {"click"};

// What a panel is declared from. The place of the style's box is the node's.
struct PanelData {
    PrKitPanelStyle style;
    bool disabled;
};

// What a selectable is declared from. The style's place is the node's.
struct SelectableData {
    PrKitBoxStyle style;
    bool selected;
    bool disabled;
};

struct PanelHeaderData {
    PrRawString meta;
};

struct SwatchData {
    JsKitColorValue color;
    float size;
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

// Reads the sides that a panel rules: one or more keywords of kRules. Returns false, and writes nothing, for any other
// text.
bool rulesFrom(PrStringView text, PrKitRuleFlags* out) {
    PrKitRuleFlags rules = 0;
    size_t at = 0;
    for (PrStringView word = nextWord(text, &at); word.size > 0; word = nextWord(text, &at)) {
        uint8_t sides = 0;
        if (!jsKitKeywordFrom(kRules, word, &sides))
            return false;
        rules |= sides;
    }
    if (rules == 0)
        return false;
    *out = rules;
    return true;
}

// Reads the digits of a color in hexadecimal notation, which follow its `#` (css-color-4/#hex-notation).
bool hexFrom(PrStringView digits, PrColor* out) {
    if (digits.size != 3 && digits.size != 4 && digits.size != 6 && digits.size != 8)
        return false;
    for (size_t i = 0; i < digits.size; ++i) {
        if (!prAsciiIsHexDigit(digits.data[i]))
            return false;
    }
    // The short notations give one digit for each channel, which stands for the same digit twice.
    size_t width = digits.size < 6 ? 1 : 2;
    uint8_t channels[4] = {0, 0, 0, 255};
    for (size_t channel = 0; channel < digits.size / width; ++channel) {
        uint32_t high = prAsciiHexValue(digits.data[channel * width]);
        uint32_t low = width == 2 ? prAsciiHexValue(digits.data[channel * width + 1]) : high;
        channels[channel] = uint8_t(high * 16 + low);
    }
    *out = prColorFromColor8(PrColor8{channels[0], channels[1], channels[2], channels[3]});
    return true;
}

// Reads three or four numbers separated by spaces as a color's red, green, blue and alpha, with an alpha of 1 if the
// text has none.
bool channelsFrom(PrStringView text, PrColor* out) {
    float channels[4] = {0, 0, 0, 1};
    uint32_t count = 0;
    size_t at = 0;
    for (PrStringView word = nextWord(text, &at); word.size > 0; word = nextWord(text, &at)) {
        if (count == 4 || !jsKitSetFloat(word, false, &channels[count]))
            return false;
        ++count;
    }
    if (count < 3)
        return false;
    *out = PrColor{channels[0], channels[1], channels[2], channels[3]};
    return true;
}

// Reads the name of a color of the palette, a space, and the color's shade, a number from -1 to 1. Returns false, and
// writes nothing, for any other text.
bool shadedFrom(PrStringView text, JsKitColorValue* out) {
    size_t space = 0;
    while (space < text.size && text.data[space] != ' ')
        ++space;
    JsKitColor named = JsKitColor_None;
    float shade = 0;
    if (space == text.size || !jsKitKeywordFrom(jsKitColors(), PrStringView{text.data, space}, &named) ||
            !jsKitSetFloat(PrStringView{text.data + space + 1, text.size - space - 1}, false, &shade) ||
            !(shade >= -1 && shade <= 1))
        return false;
    out->named = named;
    out->shade = shade;
    return true;
}

bool panelSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PanelData& panel = *static_cast<PanelData*>(data);
    if (name == "background")
        return jsKitSetKeyword(kBackgrounds, value, reset, &panel.style.background);
    if (name == "rules") {
        if (reset)
            panel.style.rules = 0;
        return reset || rulesFrom(value, &panel.style.rules);
    }
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &panel.disabled);
    return jsKitBoxStyleSet(panel.style.box, name, value, reset);
}

PrLayoutNode panelDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const PanelData& panel = jsKitData<PanelData>(round, node);
    PrKitPanelStyle style = panel.style;
    style.box.place = jsKitNodePlace(round.tree, node);
    return prKitPanel(round.kit, parent, {.key = jsKitNodeKey(round, node), .disabled = panel.disabled}, style);
}

bool selectableSet(void* data, PrStringView name, PrStringView value, bool reset) {
    SelectableData& selectable = *static_cast<SelectableData*>(data);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &selectable.selected);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &selectable.disabled);
    return jsKitBoxStyleSet(selectable.style, name, value, reset);
}

PrLayoutNode selectableDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const SelectableData& selectable = jsKitData<SelectableData>(round, node);
    PrKitBoxStyle style = selectable.style;
    style.place = jsKitNodePlace(round.tree, node);
    PrLayoutNode made = prKitSelectable(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .selected = selectable.selected,
        .disabled = selectable.disabled,
    }, style);
    if (prKitActivated(round.kit, made))
        jsKitEventPush(round, node, SelectableEvent_Click, nullptr);
    return made;
}

void panelHeaderDestroy(void* data) {
    prRawStringReset(&static_cast<PanelHeaderData*>(data)->meta);
}

bool panelHeaderSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PanelHeaderData& header = *static_cast<PanelHeaderData*>(data);
    if (name == "meta")
        return jsKitSetString(value, reset, &header.meta);
    return false;
}

PrLayoutNode panelHeaderDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const PanelHeaderData& header = jsKitData<PanelHeaderData>(round, node);
    return prKitPanelHeader(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .title = jsKitNodeText(round.tree, node),
        .meta = prRawStringView(&header.meta),
    });
}

bool swatchSet(void* data, PrStringView name, PrStringView value, bool reset) {
    SwatchData& swatch = *static_cast<SwatchData*>(data);
    if (name == "color")
        return jsKitSetColorValue(value, reset, &swatch.color);
    if (name == "size")
        return jsKitSetLength(value, reset, &swatch.size);
    return false;
}

PrLayoutNode swatchDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const SwatchData& swatch = jsKitData<SwatchData>(round, node);
    PrKitSwatch content{.key = jsKitNodeKey(round, node), .color = jsKitColorFromValue(*round.kit, swatch.color)};
    return prKitSwatch(round.kit, parent, content, {.size = swatch.size});
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "panel",
        .dataSize = sizeof(PanelData),
        .flags = JsKitKindFlag_PlacesItself,
        .set = panelSet,
        .declare = panelDeclare,
    },
    {
        .name = "selectable",
        .dataSize = sizeof(SelectableData),
        .events = kSelectableEvents,
        .eventCount = 1,
        .flags = JsKitKindFlag_PlacesItself,
        .set = selectableSet,
        .declare = selectableDeclare,
    },
    {
        .name = "panel-header",
        .dataSize = sizeof(PanelHeaderData),
        .destroy = panelHeaderDestroy,
        .set = panelHeaderSet,
        .declare = panelHeaderDeclare,
    },
    {.name = "swatch", .dataSize = sizeof(SwatchData), .set = swatchSet, .declare = swatchDeclare},
};

} // namespace

Span<const JsKitKind> jsKitPanelKinds() {
    return kKinds;
}

bool jsKitSetColorValue(PrStringView value, bool reset, JsKitColorValue* out) {
    JsKitColorValue color{};
    if (!reset) {
        if (value.size > 0 && value.data[0] == '#') {
            if (!hexFrom(PrStringView{value.data + 1, value.size - 1}, &color.own))
                return false;
        } else if (!jsKitKeywordFrom(jsKitColors(), value, &color.named) && !shadedFrom(value, &color) &&
                !channelsFrom(value, &color.own)) {
            return false;
        }
    }
    *out = color;
    return true;
}

PrColor jsKitColorFromValue(const PrKit& kit, const JsKitColorValue& value) {
    if (value.named == JsKitColor_None)
        return value.own;
    PrColor color = jsKitColorFrom(kit, value.named);
    if (value.shade == 0)
        return color;
    float target = value.shade > 0 ? 1.0f : 0.0f;
    float share = value.shade > 0 ? value.shade : -value.shade;
    return PrColor{
        color.r + (target - color.r) * share, color.g + (target - color.g) * share,
        color.b + (target - color.b) * share, color.a,
    };
}

} // namespace pr_impl
