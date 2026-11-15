// The kinds of Studio Kit's buttons (pr/kit_button.h): `button` and `badge`.
//
// A button shows its node's text, takes `variant`, `size`, `selected` and `disabled`, and queues `click` when it is
// activated. A badge shows its node's text and takes `variant`.

#include "studio/kit/quickjs/src/kit_button_quickjs.h"

#include "pr/kit_button.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kButtonVariants[] = {
    {"default", PrKitButtonVariant_Default},
    {"primary", PrKitButtonVariant_Primary},
    {"ghost", PrKitButtonVariant_Ghost},
    {"danger", PrKitButtonVariant_Danger},
};

constexpr JsKitKeyword kBadgeVariants[] = {
    {"neutral", PrKitBadgeVariant_Neutral},
    {"accent", PrKitBadgeVariant_Accent},
    {"success", PrKitBadgeVariant_Success},
    {"warning", PrKitBadgeVariant_Warning},
    {"danger", PrKitBadgeVariant_Danger},
};

// The events of a button, by their index.
enum { ButtonEvent_Click };
constexpr PrStringView kButtonEvents[] = {"click"};

struct ButtonData {
    PrKitButtonStyle style;
    bool selected;
    bool disabled;
};

bool buttonSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ButtonData& button = *static_cast<ButtonData*>(data);
    if (name == "variant")
        return jsKitSetKeyword(kButtonVariants, value, reset, &button.style.variant);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &button.style.size);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &button.selected);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &button.disabled);
    return false;
}

PrLayoutNode buttonDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const ButtonData& button = jsKitData<ButtonData>(round, node);
    PrLayoutNode made = prKitButton(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .text = jsKitNodeText(round.tree, node),
        .selected = button.selected,
        .disabled = button.disabled,
    }, button.style);
    if (prKitActivated(round.kit, made))
        jsKitEventPush(round, node, ButtonEvent_Click, nullptr);
    return made;
}

bool badgeSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitBadgeStyle& style = *static_cast<PrKitBadgeStyle*>(data);
    if (name == "variant")
        return jsKitSetKeyword(kBadgeVariants, value, reset, &style.variant);
    return false;
}

PrLayoutNode badgeDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitBadge badge{.key = jsKitNodeKey(round, node), .text = jsKitNodeText(round.tree, node)};
    return prKitBadge(round.kit, parent, badge, jsKitData<PrKitBadgeStyle>(round, node));
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "button",
        .dataSize = sizeof(ButtonData),
        .events = kButtonEvents,
        .eventCount = 1,
        .set = buttonSet,
        .declare = buttonDeclare,
    },
    {.name = "badge", .dataSize = sizeof(PrKitBadgeStyle), .set = badgeSet, .declare = badgeDeclare},
};

} // namespace

Span<const JsKitKind> jsKitButtonKinds() {
    return kKinds;
}

} // namespace pr_impl
