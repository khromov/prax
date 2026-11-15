// The kinds of Studio Kit's title bar (pr/kit_title_bar.h): `title-bar`.
//
// A `title-bar` (prKitTitleBar) shows no text and queues no event. The nodes under it are declared in the row that
// holds the program's content, and the node's placement places that row, which is the node that prKitTitleBar returns.
// The system acts on the window's buttons that the bar draws, so the script hears nothing of them.
//
// It takes the fields of PrKitTitleBar. Those of its `bar`, the window's title bar as the frame reports it, are
// `bar-height`, `bar-leading` and `bar-trailing`, each in px, and `program-buttons`, a flag, which is
// PrAppTitleBarFlag_ProgramButtons. `maximized` is a flag. The names of the bar's lengths begin with `bar-` because
// `height` is the node's placement. The binding knows nothing of the window, so the host tells the script the bar's
// geometry and whether the window is maximized, and the script sets them. Reset, the bar's height is 0, which is a
// window without a custom title bar: a plain row that drags nothing.

#include "studio/kit/quickjs/src/kit_title_bar_quickjs.h"

#include "pr/app_window.h"
#include "pr/kit_title_bar.h"

namespace pr_impl {

namespace {

bool titleBarSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitTitleBar& titleBar = *static_cast<PrKitTitleBar*>(data);
    if (name == "bar-height")
        return jsKitSetLength(value, reset, &titleBar.bar.height);
    if (name == "bar-leading")
        return jsKitSetLength(value, reset, &titleBar.bar.leading);
    if (name == "bar-trailing")
        return jsKitSetLength(value, reset, &titleBar.bar.trailing);
    if (name == "program-buttons") {
        bool drawn = false;
        if (!jsKitSetFlag(value, reset, &drawn))
            return false;
        titleBar.bar.flags = drawn ? PrAppTitleBarFlag_ProgramButtons : 0;
        return true;
    }
    if (name == "maximized")
        return jsKitSetFlag(value, reset, &titleBar.maximized);
    return false;
}

// The node's data is the widget's content, but for its key, which is the node's.
PrLayoutNode titleBarDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitTitleBar titleBar = jsKitData<PrKitTitleBar>(round, node);
    titleBar.key = jsKitNodeKey(round, node);
    return prKitTitleBar(round.kit, parent, titleBar);
}

constexpr JsKitKind kKinds[] = {
    {.name = "title-bar", .dataSize = sizeof(PrKitTitleBar), .set = titleBarSet, .declare = titleBarDeclare},
};

} // namespace

Span<const JsKitKind> jsKitTitleBarKinds() {
    return kKinds;
}

} // namespace pr_impl
