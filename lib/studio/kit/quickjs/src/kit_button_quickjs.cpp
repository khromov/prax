// The kinds of Studio Kit's buttons (pr/kit_button.h): `button`, `badge`, `kbd`, `status`, `chip` and `button-group`.
//
// Each kind but the button group shows its node's text, and the nodes under any of them are declared in its widget,
// after the widget's own parts.
//
// A `button` (prKitButton) takes `icon`, the name of an icon of the kit's set (prKitIconName), `selected` and
// `disabled`, flags, and the fields of PrKitButtonStyle: `variant`, a keyword of PrKitButtonVariant, and `size`, a
// keyword of PrKitControlSize. A button with an icon and no text is as wide as it is tall. It queues `click`, with no
// detail, when it is activated. The script sets `selected` in answer if the button is a toggle.
//
// A `badge` (prKitBadge) takes `variant`, a keyword of PrKitBadgeVariant. It queues no event.
//
// A `kbd` (prKitKbd) is a key cap. It takes no attributes of its own and queues no event.
//
// A `status` (prKitStatus) takes `tone`, a keyword of PrKitTone. It queues no event.
//
// A `chip` (prKitChip) shows its node's text as its label, and takes `removable` and `disabled`, flags. A removable
// chip queues `remove`, with no detail, when its remove control is activated. The script removes the chip in answer:
// the widget stays until it does.
//
// A `button-group` (prKitButtonGroup) takes `disabled`, a flag, which disables the buttons in it. It shows no text and
// queues no event. The `button` nodes directly under it share its border.

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

// The events of a chip, by their index.
enum { ChipEvent_Remove };
constexpr PrStringView kChipEvents[] = {"remove"};

struct ButtonData {
    PrKitButtonStyle style;
    PrKitIconKind icon;
    bool selected;
    bool disabled;
};

struct ChipData {
    bool removable;
    bool disabled;
};

struct ButtonGroupData {
    bool disabled;
};

bool buttonSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ButtonData& button = *static_cast<ButtonData*>(data);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &button.icon);
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
        .icon = button.icon,
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

PrLayoutNode kbdDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    return prKitKbd(round.kit, parent, {.key = jsKitNodeKey(round, node), .text = jsKitNodeText(round.tree, node)});
}

bool statusSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitStatusStyle& style = *static_cast<PrKitStatusStyle*>(data);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &style.tone);
    return false;
}

PrLayoutNode statusDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitStatus status{.key = jsKitNodeKey(round, node), .text = jsKitNodeText(round.tree, node)};
    return prKitStatus(round.kit, parent, status, jsKitData<PrKitStatusStyle>(round, node));
}

bool chipSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ChipData& chip = *static_cast<ChipData*>(data);
    if (name == "removable")
        return jsKitSetFlag(value, reset, &chip.removable);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &chip.disabled);
    return false;
}

PrLayoutNode chipDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const ChipData& chip = jsKitData<ChipData>(round, node);
    PrLayoutNode made = prKitChip(round.kit, parent, {
        .key = jsKitNodeKey(round, node),
        .label = jsKitNodeText(round.tree, node),
        .removable = chip.removable,
        .disabled = chip.disabled,
    });
    if (prKitRemoved(round.kit, made))
        jsKitEventPush(round, node, ChipEvent_Remove, nullptr);
    return made;
}

bool buttonGroupSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ButtonGroupData& group = *static_cast<ButtonGroupData*>(data);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &group.disabled);
    return false;
}

PrLayoutNode buttonGroupDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const ButtonGroupData& group = jsKitData<ButtonGroupData>(round, node);
    return prKitButtonGroup(round.kit, parent, {.key = jsKitNodeKey(round, node), .disabled = group.disabled});
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
    {.name = "kbd", .declare = kbdDeclare},
    {.name = "status", .dataSize = sizeof(PrKitStatusStyle), .set = statusSet, .declare = statusDeclare},
    {
        .name = "chip",
        .dataSize = sizeof(ChipData),
        .events = kChipEvents,
        .eventCount = 1,
        .set = chipSet,
        .declare = chipDeclare,
    },
    {.name = "button-group", .dataSize = sizeof(ButtonGroupData), .set = buttonGroupSet, .declare = buttonGroupDeclare},
};

} // namespace

Span<const JsKitKind> jsKitButtonKinds() {
    return kKinds;
}

} // namespace pr_impl
