// The kinds of Studio Kit's buttons in its QuickJS binding: each declares what the same calls to the kit declare.

#include "pr/array.h"
#include "pr/count_of.h"
#include "pr/kit_box.h"
#include "pr/kit_button.h"
#include "pr/kit_text.h"

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

TEST_CASE("a button shows an icon, text or both, and a toggle shows the selection that the script sets on a click") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '8', padding: '8'});
        var tool = make('button', row, {icon: 'move', variant: 'ghost', size: 'sm'});
        make('button', row, {icon: 'plus', variant: 'primary', size: 'lg'}, 'Add');
        make('button', row, {icon: 'trash', variant: 'danger', size: 'xs', disabled: ''}, 'Delete');
        make('button', row, {selected: '', margin: '0 0 0 16'}, 'On');
        var on = false;
        var details = 0;
        kitTree.listen(tool, 'click', true);
        kitTree.onevent = function (node, type, detail) {
            if (node !== tool || type !== 'click')
                throw new Error('an event of another node');
            on = !on;
            details += Object.keys(detail).length;
            kitTree.set(tool, 'selected', on ? '' : null);
        };
    )JS");
    KitFixture direct;
    PrLayoutNode tool = kPrLayoutNodeNone;
    bool on = false;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .gap = 8,
            .padding = prKitEdgesAll(8),
        });
        tool = prKitButton(kit, row, {.icon = PrKitIconKind_Move, .selected = on}, {
            .variant = PrKitButtonVariant_Ghost,
            .size = PrKitControlSize_Sm,
        });
        if (prKitActivated(kit, tool))
            on = !on;
        prKitButton(kit, row, {.text = "Add", .icon = PrKitIconKind_Plus}, {
            .variant = PrKitButtonVariant_Primary,
            .size = PrKitControlSize_Lg,
        });
        prKitButton(kit, row, {.text = "Delete", .icon = PrKitIconKind_Trash, .disabled = true}, {
            .variant = PrKitButtonVariant_Danger,
            .size = PrKitControlSize_Xs,
        });
        PrLayoutNode toggled = prKitButton(kit, row, {.text = "On", .selected = true}, {});
        prKitPlace(kit, toggled, {.margin = PrKitEdges{0, 0, 0, 16}});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // A click toggles the tool on in both, and Space, while it has the focus, toggles it off again.
    PrVec2 at = direct.center(tool);
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(on);
    CHECK(scripted.number("on ? 1 : 0") == 1);
    CHECK(scripted.number("details") == 0);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    scripted.fixture.key(' ');
    direct.key(' ');
    frames(scripted, direct);
    CHECK(!on);
    CHECK(scripted.number("on ? 1 : 0") == 0);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a badge, a key cap and a status show their text as the kit's tags") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', align: 'center', gap: '6', padding: '8', wrap: ''});
        ['neutral', 'accent', 'success', 'warning', 'danger'].forEach(function (variant) {
            make('badge', row, {variant: variant}, variant);
        });
        ['Ctrl', 'Shift', 'P'].forEach(function (cap) { make('kbd', row, {}, cap); });
        var states = {M: 'warning', A: 'success', D: 'danger', R: 'accent', U: 'auto'};
        for (const state in states)
            make('status', row, {tone: states[state]}, state);
        make('kbd', row, {margin: '0 0 0 12'}, 'Esc');
    )JS");
    KitFixture direct;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row = prKitBox(kit, root, {}, {
            .direction = PrKitDirection_Row,
            .align = PrKitAlign_Center,
            .wrap = true,
            .gap = 6,
            .padding = prKitEdgesAll(8),
        });
        const char* variants[] = {"neutral", "accent", "success", "warning", "danger"};
        for (uint32_t i = 0; i < countOf(variants); ++i)
            prKitBadge(kit, row, {.text = variants[i]}, {.variant = PrKitBadgeVariant(i)});
        const char* caps[] = {"Ctrl", "Shift", "P"};
        for (const char* cap : caps)
            prKitKbd(kit, row, {.text = cap});
        prKitStatus(kit, row, {.text = "M"}, {.tone = PrKitTone_Warning});
        prKitStatus(kit, row, {.text = "A"}, {.tone = PrKitTone_Success});
        prKitStatus(kit, row, {.text = "D"}, {.tone = PrKitTone_Danger});
        prKitStatus(kit, row, {.text = "R"}, {.tone = PrKitTone_Accent});
        prKitStatus(kit, row, {.text = "U"}, {});
        PrLayoutNode escape = prKitKbd(kit, row, {.text = "Esc"});
        prKitPlace(kit, escape, {.margin = PrKitEdges{0, 0, 0, 12}});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("a chip queues a remove when its remove control is activated, and stays until the script removes it") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '6', padding: '8'});
        var chips = ['player', 'enemy', 'static'].map(function (label) {
            const chip = make('chip', row, {removable: ''}, label);
            kitTree.listen(chip, 'remove', true);
            return chip;
        });
        make('chip', row, {}, 'read-only');
        make('chip', row, {removable: '', disabled: ''}, 'locked');
        var removed = [];
        var details = 0;
        var removing = true;
        kitTree.onevent = function (node, type, detail) {
            if (type !== 'remove')
                throw new Error('an event of another type');
            removed.push(chips.indexOf(node));
            details += Object.keys(detail).length;
            if (removing)
                kitTree.destroy(node);
        };
    )JS");
    KitFixture direct;
    Array<const char*> labels;
    labels.push("player");
    labels.push("enemy");
    labels.push("static");
    PrLayoutNode chips[3] = {};
    PrLayoutNode locked = kPrLayoutNodeNone;
    uint32_t removals = 0;
    bool removing = true;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row =
            prKitBox(kit, root, {}, {.direction = PrKitDirection_Row, .gap = 6, .padding = prKitEdgesAll(8)});
        uint32_t gone = labels.count();
        for (uint32_t i = 0; i < labels.count(); ++i) {
            chips[i] = prKitChip(kit, row, {.label = labels[i], .removable = true});
            if (prKitRemoved(kit, chips[i]))
                gone = i;
        }
        prKitChip(kit, row, {.label = "read-only"});
        locked = prKitChip(kit, row, {.label = "locked", .removable = true, .disabled = true});
        removals += prKitRemoved(kit, locked) ? 1 : 0;
        if (gone < labels.count()) {
            ++removals;
            if (removing)
                labels.erase(gone);
        }
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    // The remove control is the chip's last child, at its end. The script destroys the chip that the event names.
    PrRect chip = direct.bounds(chips[1]);
    PrVec2 at{chip.right - 8, (chip.top + chip.bottom) / 2};
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(removals == 1);
    CHECK(labels.count() == 2);
    CHECK(scripted.number("removed.length") == 1);
    CHECK(scripted.number("removed[0]") == 1);
    CHECK(scripted.number("details") == 0);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A chip that the script leaves in place stays, and a click on its label or on a disabled chip's control removes
    // nothing.
    scripted.run("removing = false;");
    removing = false;
    chip = direct.bounds(chips[0]);
    at = PrVec2{chip.right - 8, (chip.top + chip.bottom) / 2};
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(removals == 2);
    CHECK(scripted.number("removed.length") == 2);
    CHECK(scripted.number("removed[1]") == 0);
    at = PrVec2{chip.left + 8, (chip.top + chip.bottom) / 2};
    scripted.fixture.click(at);
    direct.click(at);
    chip = direct.bounds(locked);
    at = PrVec2{chip.right - 8, (chip.top + chip.bottom) / 2};
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct, 30);
    CHECK(removals == 2);
    CHECK(scripted.number("removed.length") == 2);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("the buttons under a button group share its border, and a disabled group disables them") {
    JsKitScripted scripted;
    scripted.run(kJsKitMake);
    scripted.run(R"JS(
        var row = make('box', 0, {direction: 'row', gap: '12', padding: '8'});
        var spaces = make('button-group', row, {});
        var local = make('button', spaces, {selected: ''}, 'Local');
        var world = make('button', spaces, {}, 'World');
        var icons = make('button-group', row, {disabled: '', 'align-self': 'center'});
        make('button', icons, {icon: 'undo', size: 'sm'});
        make('button', icons, {icon: 'redo', size: 'sm'});
        var clicked = [];
        kitTree.listen(local, 'click', true);
        kitTree.listen(world, 'click', true);
        kitTree.onevent = function (node, type) {
            clicked.push(type + (node === world ? ' world' : ' local'));
            kitTree.set(local, 'selected', node === local ? '' : null);
            kitTree.set(world, 'selected', node === world ? '' : null);
        };
    )JS");
    KitFixture direct;
    PrLayoutNode world = kPrLayoutNodeNone;
    bool global = false;
    bool grouped = true;
    direct.build = [&](PrLayoutNode root) {
        PrKit* kit = direct.kit;
        PrLayoutNode row =
            prKitBox(kit, root, {}, {.direction = PrKitDirection_Row, .gap = 12, .padding = prKitEdgesAll(8)});
        PrLayoutNode spaces = grouped ? prKitButtonGroup(kit, row, {}) : prKitBox(kit, row, {}, {});
        PrLayoutNode local = prKitButton(kit, spaces, {.text = "Local", .selected = !global}, {});
        world = prKitButton(kit, spaces, {.text = "World", .selected = global}, {});
        if (prKitActivated(kit, local))
            global = false;
        if (prKitActivated(kit, world))
            global = true;
        PrLayoutNode icons = prKitButtonGroup(kit, row, {.disabled = true});
        prKitPlace(kit, icons, {.align = PrKitAlign_Center});
        prKitButton(kit, icons, {.icon = PrKitIconKind_Undo}, {.size = PrKitControlSize_Sm});
        prKitButton(kit, icons, {.icon = PrKitIconKind_Redo}, {.size = PrKitControlSize_Sm});
    };
    frames(scripted, direct);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));

    PrVec2 at = direct.center(world);
    scripted.fixture.click(at);
    direct.click(at);
    frames(scripted, direct);
    CHECK(global);
    CHECK(scripted.number("clicked.length") == 1);
    CHECK(scripted.number("clicked[0] === 'click world' ? 1 : 0") == 1);
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));

    // A button that the script moves out of the group draws its own border again: under a box in place of the group,
    // the buttons look as the kit's own do there.
    scripted.run(R"JS(
        var plain = make('box', row, {});
        kitTree.insert(row, plain, spaces);
        kitTree.insert(plain, local, 0);
        kitTree.insert(plain, world, 0);
        kitTree.destroy(spaces);
    )JS");
    grouped = false;
    frames(scripted, direct, 30);
    CHECK(jsKitRendersAlike(scripted.fixture, direct));
    CHECK(kitVerified(scripted.fixture.tree));
}

TEST_CASE("the kinds of buttons take only their own attributes and events") {
    JsKitScripted scripted;
    scripted.run(R"JS(
        var button = kitTree.create('button');
        var kbd = kitTree.create('kbd');
        var status = kitTree.create('status');
        var chip = kitTree.create('chip');
        var group = kitTree.create('button-group');
        var taken = [
            kitTree.set(button, 'icon', 'settings'), kitTree.set(button, 'icon', null),
            kitTree.set(kbd, 'margin', '2'), kitTree.set(status, 'tone', 'axis-z'), kitTree.set(chip, 'removable', ''),
            kitTree.set(chip, 'disabled', 'true'), kitTree.set(group, 'disabled', ''),
            kitTree.listen(chip, 'remove', true), kitTree.listen(button, 'click', true),
        ].filter(Boolean).length;
        var refused = [
            kitTree.set(button, 'icon', 'gear'), kitTree.set(button, 'icon', ''), kitTree.set(button, 'removable', ''),
            kitTree.set(kbd, 'variant', 'accent'), kitTree.set(kbd, 'text', 'Esc'), kitTree.set(status, 'tone', 'M'),
            kitTree.set(status, 'variant', 'accent'), kitTree.set(chip, 'removable', 'no'),
            kitTree.set(chip, 'label', 'player'), kitTree.set(chip, 'variant', 'accent'),
            kitTree.set(group, 'gap', '4'), kitTree.set(group, 'size', 'sm'), kitTree.listen(chip, 'click', true),
            kitTree.listen(button, 'remove', true), kitTree.listen(kbd, 'click', true),
            kitTree.listen(status, 'click', true), kitTree.listen(group, 'click', true),
        ].filter(Boolean).length;
    )JS");
    CHECK(scripted.number("taken") == 9);
    CHECK(scripted.number("refused") == 0);
}
