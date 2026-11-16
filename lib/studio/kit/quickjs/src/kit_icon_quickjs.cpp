// The kind of Studio Kit's icons (pr/kit_icon.h) that is written by hand: `path-icon`. An `icon` is described
// (kit_node_quickjs_kinds.txt).
//
// A `path-icon` (prKitPathIcon) shows no text and queues no event, and the nodes under it are not declared, since an
// icon holds nothing. It takes `path`, SVG path data on the set's grid of 24 units (SVG2/paths.html#PathData), `color`,
// a color as a `swatch` takes one, `stroked`, a flag, and `size`, the side of its square in px, where 0 is the set's 16
// px. It takes no `tone`. A path that has an error is refused whole. Reset, `path` is an empty path and `color` is
// transparent, so the square is empty until the script gives both.

#include "studio/kit/quickjs/src/kit_icon_quickjs.h"

#include "pr/alloc.h"
#include "pr/kit_icon.h"
#include "pr/memory.h"
#include "pr/path.h"
#include "pr/path_data.h"
#include "pr/status.h"
#include "pr/ui.h"

namespace pr_impl {

namespace {

// A path that a path icon draws, shared by the node that the script gave it to and by each widget that was last
// declared with it. A widget keeps a pointer to its path until a later round declares it with another path or does not
// declare it, which is after its node's script may have replaced the path or destroyed the node. The count is plain,
// since a tree and the kit that declares it are used from one thread.
struct IconPath {
    uint32_t references;
    PrPath* path;
};

// What a path icon is declared from.
struct PathIconData {
    IconPath* path;                 // never null once the node is created
    JsKitColorValue color;
    float size;
    bool stroked;
};

// The owner of the state that a path icon's widget keeps (prUiState).
constexpr char kPathOwner = 0;

// Creates an empty path with one reference.
IconPath* pathCreate() {
    IconPath* made = allocZeroed<IconPath>();
    made->references = 1;
    made->path = prPathCreate();
    return made;
}

// Releases a reference to a path, which the last release frees. Null is no path.
void pathRelease(IconPath* PR_NULLABLE shared) {
    if (shared == nullptr || --shared->references > 0)
        return;
    prPathDestroy(shared->path);
    prMemoryFree(shared);
}

// Releases the path that a widget's state holds, as the state is freed.
void keptRelease(void* state) {
    pathRelease(*static_cast<IconPath**>(state));
}

// Places an icon's widget as the script placed its node. An icon is a replaced element, which holds no nodes, so its
// kind places it and returns no node for the tree to declare the node's children under.
void place(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode made) {
    if (jsKitNodeIsPlaced(round.tree, node))
        prKitPlace(round.kit, made, jsKitNodePlace(round.tree, node));
}

void pathIconCreate(void* data) {
    static_cast<PathIconData*>(data)->path = pathCreate();
}

void pathIconDestroy(void* data) {
    pathRelease(static_cast<PathIconData*>(data)->path);
}

bool pathIconSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PathIconData& icon = *static_cast<PathIconData*>(data);
    if (name == "path") {
        // The new path is made while the old one exists, so that it is at another address: the kit draws an icon again
        // only if its path's address changed.
        IconPath* made = pathCreate();
        if (!reset && prPathAppendData(made->path, value) != PrStatus_Ok) {
            pathRelease(made);
            return false;
        }
        pathRelease(icon.path);
        icon.path = made;
        return true;
    }
    if (name == "color")
        return jsKitSetColorValue(value, reset, &icon.color);
    if (name == "stroked")
        return jsKitSetFlag(value, reset, &icon.stroked);
    if (name == "size")
        return jsKitSetLength(value, reset, &icon.size);
    return false;
}

PrLayoutNode pathIconDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const PathIconData& icon = jsKitData<PathIconData>(round, node);
    PrKit* kit = round.kit;
    PrLayoutNode made = prKitPathIcon(kit, parent, {
        .key = jsKitNodeKey(round, node),
        .path = icon.path->path,
        .color = jsKitColorFromValue(*kit, icon.color),
        .stroked = icon.stroked,
    }, {.size = icon.size});
    // The widget's state holds a reference to the path that the widget was declared with. The context frees the state
    // at the end of the first round that does not declare the widget, which is when the widget lets go of the path.
    IconPath** kept = static_cast<IconPath**>(
        prUiStateWithDestroy(kit->ui, made, &kPathOwner, 0, uint32_t(sizeof(IconPath*)), keptRelease));
    if (*kept != icon.path) {
        pathRelease(*kept);
        ++icon.path->references;
        *kept = icon.path;
    }
    place(round, node, made);
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "path-icon",
        .dataSize = sizeof(PathIconData),
        .create = pathIconCreate,
        .destroy = pathIconDestroy,
        .set = pathIconSet,
        .declare = pathIconDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitIconKinds() {
    return kKinds;
}

} // namespace pr_impl
