// The kinds of Studio Kit's collections (pr/kit_collection.h) that are written by hand: the views `list-view`,
// `tree-view` and `table`, their rows `list-row`, `list-tile`, `tree-row` and `table-row`, a table's `table-column` and
// `table-cell`, and `tile-image`, `visibility-toggle` and `drop-zone`. The other parts of a row and of a tile, and
// `empty-state`, are described (kit_node_quickjs_kinds.txt).
//
// Rows and windows. The script makes every row of a view as a node under the view's node. A view's rows are the nodes
// of its row kinds under it, in the tree's order, and a row's index is its place among them. In each round a view asks
// the kit for its window (prKitListWindow) and declares the rows of the window alone, each with the nodes under it. A
// round therefore costs a view what its window costs, however many rows the view has. A view keeps a list of its rows.
// It reads the list again in the first round after the script edits any node of the tree, which walks the view's
// children once. A row is keyed by its node, so it keeps its widget wherever the script moves it. A row takes its size
// from its view, and a cell from its column, so the placement attributes do nothing on a row, a tile or a cell. A node
// of another kind under a view is declared under the view's widget, after the view's rows. A row, a tile, a column or a
// cell that is not under a node of the kind that holds it declares nothing.
//
// Selection. A row is selected if its `selected` is set. The flag belongs to the row's node, so the selection follows
// the rows as the script reorders them. A view with `selectable` takes focus, and clicks on its rows and its keys edit
// the selection as pr/kit_collection.h describes. The view's `anchor` and `lead` are the rows of the selection's
// cursor, which the kit writes and a script may set. The view queues each edit as `select`. The event's `text` is
// `replace`, `add` or `toggle`. Its `values` are the edit's first row, the number of its rows, and the lead and the
// anchor after the edit. A script that listens for `select` applies the edit to its rows: for `replace` it clears every
// row's `selected` and sets it on the edit's rows, for `add` it sets it on them, and for `toggle` it inverts it on
// them. If no script listens for a view's `select`, the view applies the edit to its rows itself (prKitSelectionApply),
// so it selects with no script. Applying an edit reads and writes every row's flag once.
//
// Trees. A tree's rows are flat, as the kit takes them. Every `tree-row` is a child of its `tree-view`, in display
// order, and its `depth` places it in the tree: a row's parent is the nearest row before it with a lesser depth. A row
// shows a disclosure if its `has-children` is set or the row after it is deeper. The rows under a row that is not
// `open` do not show. The view leaves them out of its rows, so they are not declared, and every index of a row, in the
// view's attributes and in its events, counts the rows that show. A script either keeps the rows that do not show under
// the view or removes them. An edit that replaces the selection clears the rows that do not show too. A click on a
// disclosure, or Left or Right on the lead, asks to open or close a row, which the view queues as `fold`. The event's
// `values` are the row, and 1 to open it or 0 to close it, and the script sets the row's `open`. If no script listens
// for the view's `fold`, the view sets the row's `open` itself.
//
// Sorting and drops. A table keeps the column that it is sorted by in `sort-column` and `sort-descending`, which mark
// that column's heading. A click on the heading of a `sortable` column writes them, and the table queues `sort`, whose
// `values` are the column and 1 if the order is descending, or 0. The script orders the table's rows by moving their
// nodes. A tree view with a `drag-kind` lets its rows drag, and takes their drops. A `drag-ghost` under a row shows
// beside the pointer while that row is the one dragged. The view queues a drop as `drop`. The event's `text` is
// `before`, `inside` or `after`, which is where the dragged row lands by the row that it was dropped on. Its `values`
// are the row dropped on, the dragged row, which is -1 if the payload is not a row that the view shows, and the
// payload's value, which for a row is its node. The script moves the dragged row's node, and sets the depths of the
// rows that move with it.
//
// The kinds, each with the attributes that it takes besides the placement that every kind takes:
//
// `list-view` takes `row-height`, the height of its rows in px, `tile-width`, which if it is above zero makes the view
// a grid of tiles that wide at least, `selectable`, `anchor` and `lead`. Its rows are its `list-row` and `list-tile`
// children. It queues `select`. The node's placement is the view's.
//
// `list-row` and `list-tile` take `selected`. A row is a line of the nodes under it, and a tile a column of them. Each
// queues `click` when it is activated, with its index as the event's `value`. A click that edits the selection is
// queued before the view's `select`.
//
// `tile-image` takes `hover-zoom`, the fraction by which its picture grows under the pointer, and `image`, the picture
// as three values separated by spaces: its width and its height in pixels, each at most 4096, and its pixels in base64,
// which are RGBA8 with straight alpha in sRGB, row by row. A `tile-image` with no picture declares nothing. It is a
// tile's part, and goes under a `list-tile`.
//
// `tree-view` takes `row-height`, `selectable`, `anchor`, `lead` and `drag-kind`, a number that is not zero if its rows
// drag. Its rows are its `tree-row` children. It queues `select`, `fold` and `drop`. The node's placement is the
// view's.
//
// `tree-row` takes `selected`, `depth`, `open` and `has-children`, and holds its cells as a `list-row` does. It queues
// `click` as a `list-row` does. A click on its disclosure is not a click on the row.
//
// `visibility-toggle` takes `hidden` and `revealed`. It shows its eye if it is hidden or revealed. `revealed` is a
// flag, or `hover`, which reveals the toggle while the pointer is over the widget that the toggle is under, as a row's
// toggle is revealed. It queues `click` when it is activated, and the script sets `hidden`.
//
// `drop-zone` takes `drag-kind` and declares no widget. It makes the widget that it is under take the drops of payloads
// of that kind, and tells where each lands by the third of the widget that the pointer is over (prKitDropZone). It
// queues `drop` with `before`, `inside` or `after` as the event's `text` and the payload's value as its `value`, which
// for a tree's row is the row's node. It queues `dragover` each time the place where a dragged payload would land
// changes, with that place as the event's `text`, which is `none` once the payload leaves the widget or drops.
//
// `table` takes `row-height`, `selectable`, `anchor`, `lead`, `sort-column`, which is no column unless it is set, and
// `sort-descending`. Its columns are its `table-column` children, in order, and its rows its `table-row` children. It
// queues `select` and `sort`. The node's placement is the table's.
//
// `table-column` shows its node's text as its heading, and takes `align`, which is `start`, `center` or `end`, and
// `sortable`. Its placement gives the column's width: `width` in px, and with `flex` set to `fill` a share of the room
// that the other columns leave, by `weight`. The rest of its placement does nothing.
//
// `table-row` takes `selected`, and queues `click` as a `list-row` does. Its `table-cell` children are its cells, and a
// node of another kind under it is declared in the row after them.
//
// `table-cell` shows its node's text, and takes `icon`, `role`, `tone` and `column`. A cell is in the column of its
// place among its row's cells unless `column` names another. A cell whose column the table does not have is not
// declared. The nodes under a cell are declared in it after its text.

#include "studio/kit/quickjs/src/kit_collection_quickjs.h"

#include "pr/assert.h"
#include "pr/base64.h"
#include "pr/count_of.h"
#include "pr/draw_image.h"
#include "pr/kit_collection.h"
#include "pr/memory.h"
#include "pr/raw_array.h"

namespace pr_impl {

namespace {

// The family's kinds, each by its index among them.
enum {
    Kind_ListView,
    Kind_ListRow,
    Kind_ListTile,
    Kind_TileImage,
    Kind_TreeView,
    Kind_TreeRow,
    Kind_VisibilityToggle,
    Kind_DropZone,
    Kind_Table,
    Kind_TableColumn,
    Kind_TableRow,
    Kind_TableCell,
    Kind_Count,
};
typedef uint8_t Kind;

// Returns true if `node` is a node of `kind`.
bool isKind(const JsKitNodeTree* tree, JsKitNodeIndex node, Kind kind);

constexpr JsKitKeyword kColumnAlignments[] = {
    {"start", PrKitTextAlign_Start},
    {"center", PrKitTextAlign_Center},
    {"end", PrKitTextAlign_End},
};

// How an edit's rows join the selection, and where a drop lands, as an event's text names them.
constexpr JsKitKeyword kSelectionOps[] = {
    {"replace", PrKitSelectionOp_Replace},
    {"add", PrKitSelectionOp_Add},
    {"toggle", PrKitSelectionOp_Toggle},
};
constexpr JsKitKeyword kDropZones[] = {
    {"none", PrKitDropZone_None},
    {"before", PrKitDropZone_Before},
    {"inside", PrKitDropZone_Inside},
    {"after", PrKitDropZone_After},
};

// The events of the views, by their index. Every view queues `select` first.
enum { ViewEvent_Select, TreeEvent_Fold, TreeEvent_Drop, TableEvent_Sort = 1 };
constexpr PrStringView kListEvents[] = {"select"};
constexpr PrStringView kTreeEvents[] = {"select", "fold", "drop"};
constexpr PrStringView kTableEvents[] = {"select", "sort"};

// The event of a row, a tile and a visibility toggle.
enum { PartEvent_Click };
constexpr PrStringView kClickEvents[] = {"click"};

// The events of a drop zone, by their index.
enum { DropEvent_Drop, DropEvent_Dragover };
constexpr PrStringView kDropEvents[] = {"drop", "dragover"};

// The deepest level of a tree's row. A row is indented by its depth, and this keeps the indent a length that a layout
// measures.
constexpr uint32_t kTreeMaxDepth = 0xffff;

// The absent depth of a row.
constexpr uint32_t kDepthNone = 0xffffffffu;

// The most pixels along a side of the picture that a script gives a tile.
constexpr uint32_t kImageMaxSide = 4096;

// Declares nothing. A view declares its rows, a row its cells, and a table reads its columns, so this is the declare of
// those kinds, which the tree calls only for a node that is not under a node of the kind that holds it.
PrLayoutNode declareNothing(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

// ---------------------------------------------------------------------------------------------------------------------
// Rows

// A row or a tile of a view. `depth`, `open` and `hasChildren` place a tree's row in its tree.
struct RowData {
    bool selected;
    bool open;
    bool hasChildren;
    uint32_t depth;
};

RowData& rowOf(const JsKitNodeTree* tree, JsKitNodeIndex node) {
    return *static_cast<RowData*>(jsKitNodeData(tree, node));
}

bool rowSet(void* data, PrStringView name, PrStringView value, bool reset) {
    RowData& row = *static_cast<RowData*>(data);
    if (name == "selected")
        return jsKitSetFlag(value, reset, &row.selected);
    return false;
}

bool treeRowSet(void* data, PrStringView name, PrStringView value, bool reset) {
    RowData& row = *static_cast<RowData*>(data);
    if (name == "depth") {
        uint32_t depth = 0;
        if (!jsKitSetUint(value, reset, &depth) || depth > kTreeMaxDepth)
            return false;
        row.depth = depth;
        return true;
    }
    if (name == "open")
        return jsKitSetFlag(value, reset, &row.open);
    if (name == "has-children")
        return jsKitSetFlag(value, reset, &row.hasChildren);
    return rowSet(data, name, value, reset);
}

// Queues the click of the row or tile at `index` of its view, whose widget is `made`, if the round activated it.
void rowClick(const JsKitRound& round, JsKitNodeIndex node, uint32_t index, PrLayoutNode made) {
    if (!prKitActivated(round.kit, made))
        return;
    JsKitDetail detail{.values = {double(index)}, .valueCount = 1};
    jsKitEventPush(round, node, PartEvent_Click, &detail);
}

// ---------------------------------------------------------------------------------------------------------------------
// Views

// A list view, a tree view or a table: its attributes, the storage that the kit writes, and what it last read of its
// node's children.
struct ViewData {
    float rowHeight;                        // px, or zero for the height that the kit gives a row
    float tileWidth;                        // px; above zero makes a list view a grid
    bool selectable;
    uint32_t dragKind;                      // the kind of payload that a tree view's rows drag as, or zero if none drag
    PrKitSelection cursor;                  // the anchor and the lead of the selection, which the kit writes
    PrKitTableSort sort;                    // the column that a table is sorted by, which the kit writes

    // What the view read of its node's children when the tree's version was `version`, if `read` is true. `rows` holds
    // the rows that show, and `others` the children that are neither rows nor columns, both in the tree's order. A tree
    // view has one of `items` for each of `rows`. A table has its `columns` in order, and a column's label is its
    // node's text, which the tree owns until the script changes it.
    bool read;
    uint64_t version;
    RawArray<JsKitNodeIndex> rows;
    RawArray<JsKitNodeIndex> others;
    RawArray<PrKitTreeItem> items;
    RawArray<PrKitTableColumn> columns;

    RawArray<bool> flags;                   // whether each of `rows` is selected, while the view applies an edit
};

void viewDestroy(void* data) {
    ViewData& view = *static_cast<ViewData*>(data);
    view.rows.reset();
    view.others.reset();
    view.items.reset();
    view.columns.reset();
    view.flags.reset();
}

// A table is sorted by no column until its script names one.
void tableCreate(void* data) {
    static_cast<ViewData*>(data)->sort.column = kPrKitColumnNone;
}

// Reads the height of a view's rows or the width of its tiles, in px. Zero is the view's own. A length between zero and
// one px is refused, since a view counts the rows and the tiles that fit in it by these.
bool setRowExtent(PrStringView value, bool reset, float* out) {
    float extent = 0;
    if (!jsKitSetLength(value, reset, &extent) || (extent > 0 && extent < 1))
        return false;
    *out = extent;
    return true;
}

// Sets an attribute that every view takes.
bool viewSet(ViewData& view, PrStringView name, PrStringView value, bool reset) {
    if (name == "row-height")
        return setRowExtent(value, reset, &view.rowHeight);
    if (name == "selectable")
        return jsKitSetFlag(value, reset, &view.selectable);
    if (name == "anchor")
        return jsKitSetUint(value, reset, &view.cursor.anchor);
    if (name == "lead")
        return jsKitSetUint(value, reset, &view.cursor.lead);
    return false;
}

bool listViewSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ViewData& view = *static_cast<ViewData*>(data);
    if (name == "tile-width")
        return setRowExtent(value, reset, &view.tileWidth);
    return viewSet(view, name, value, reset);
}

bool treeViewSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ViewData& view = *static_cast<ViewData*>(data);
    if (name == "drag-kind")
        return jsKitSetUint(value, reset, &view.dragKind);
    return viewSet(view, name, value, reset);
}

bool tableSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ViewData& view = *static_cast<ViewData*>(data);
    if (name == "sort-column") {
        if (!jsKitSetUint(value, reset, &view.sort.column))
            return false;
        if (reset)
            view.sort.column = kPrKitColumnNone;
        return true;
    }
    if (name == "sort-descending")
        return jsKitSetFlag(value, reset, &view.sort.descending);
    return viewSet(view, name, value, reset);
}

// Reads the children of a view's node into the view's lists, which are empty.
typedef void (*ViewReadFn)(const JsKitNodeTree* tree, JsKitNodeIndex node, ViewData& view);

void listRead(const JsKitNodeTree* tree, JsKitNodeIndex node, ViewData& view) {
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isKind(tree, child, Kind_ListRow) || isKind(tree, child, Kind_ListTile))
            view.rows.push(child);
        else
            view.others.push(child);
    }
}

void treeRead(const JsKitNodeTree* tree, JsKitNodeIndex node, ViewData& view) {
    // The depth of the closed row whose rows are being left out, or none.
    uint32_t closed = kDepthNone;
    // True if the row before `child` shows, in which case it is the last of the view's rows.
    bool shown = false;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (!isKind(tree, child, Kind_TreeRow)) {
            view.others.push(child);
            continue;
        }
        const RowData& row = rowOf(tree, child);
        if (shown && row.depth > view.items.last().depth)
            view.items.last().hasChildren = true;
        shown = closed == kDepthNone || row.depth <= closed;
        if (!shown)
            continue;
        closed = row.open ? kDepthNone : row.depth;
        view.rows.push(child);
        view.items.push(PrKitTreeItem{.depth = row.depth, .hasChildren = row.hasChildren, .open = row.open});
    }
}

// A table's column: where its heading and its cells sit, and whether a click on its heading sorts by it.
struct ColumnData {
    PrKitTextAlign align;
    bool sortable;
};

bool columnSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ColumnData& column = *static_cast<ColumnData*>(data);
    if (name == "align")
        return jsKitSetKeyword(kColumnAlignments, value, reset, &column.align);
    if (name == "sortable")
        return jsKitSetFlag(value, reset, &column.sortable);
    return false;
}

void tableRead(const JsKitNodeTree* tree, JsKitNodeIndex node, ViewData& view) {
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (isKind(tree, child, Kind_TableRow)) {
            view.rows.push(child);
        } else if (isKind(tree, child, Kind_TableColumn)) {
            const ColumnData& column = *static_cast<const ColumnData*>(jsKitNodeData(tree, child));
            PrKitPlace place = jsKitNodePlace(tree, child);
            PrKitTableColumn made{
                .label = jsKitNodeText(tree, child),
                .align = column.align,
                .sortable = column.sortable,
            };
            if (place.width.kind == PrKitSizeKind_Px)
                made.width = place.width.value;
            if (place.flex == PrKitFlex_Fill) {
                made.flex = PrKitFlex_Fill;
                made.weight = place.weight;
            }
            view.columns.push(made);
        } else {
            view.others.push(child);
        }
    }
}

// Reads the children of the view's node again, if the script edited the tree since the view last read them.
void viewRefresh(const JsKitRound& round, JsKitNodeIndex node, ViewData& view, ViewReadFn read) {
    uint64_t version = jsKitTreeVersion(round.tree);
    if (view.read && view.version == version)
        return;
    view.rows.clear();
    view.others.clear();
    view.items.clear();
    view.columns.clear();
    read(round.tree, node, view);
    view.read = true;
    view.version = version;
}

// Returns the cursor that the kit edits for the view, or null for a view that selects nothing.
PrKitSelection* PR_NULLABLE viewCursor(ViewData& view) {
    return view.selectable ? &view.cursor : nullptr;
}

// Tells the script of the edit that the round made to the selection of the view whose widget is `made`, if it made one.
// A view that the script does not listen to applies the edit to its rows instead.
void viewSelect(const JsKitRound& round, JsKitNodeIndex node, ViewData& view, PrLayoutNode made) {
    PrKitSelectionEdit edit{};
    if (!prKitSelectionEdited(round.kit, made, &edit) || view.rows.count == 0)
        return;
    const JsKitNodeTree* tree = round.tree;
    if (jsKitNodeListens(tree, node, ViewEvent_Select)) {
        JsKitDetail detail{
            .values = {double(edit.first), double(edit.count), double(view.cursor.lead), double(view.cursor.anchor)},
            .valueCount = 4,
            .hasText = true,
            .text = jsKitKeywordName(kSelectionOps, edit.op),
        };
        jsKitEventPush(round, node, ViewEvent_Select, &detail);
        return;
    }
    // The rows that a tree view leaves out are in no edit, and an edit that replaces the selection clears them too.
    if (edit.op == PrKitSelectionOp_Replace && isKind(tree, node, Kind_TreeView)) {
        for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
                child = jsKitNodeNextSibling(tree, child)) {
            if (isKind(tree, child, Kind_TreeRow))
                rowOf(tree, child).selected = false;
        }
    }
    view.flags.resize(view.rows.count);
    for (uint32_t i = 0; i < view.rows.count; ++i)
        view.flags.data[i] = rowOf(tree, view.rows.data[i]).selected;
    prKitSelectionApply(&edit, view.flags.data, view.rows.count);
    for (uint32_t i = 0; i < view.rows.count; ++i)
        rowOf(tree, view.rows.data[i]).selected = view.flags.data[i];
}

// Declares the children of a view's node that are not its rows under the view's widget.
void viewDeclareOthers(const JsKitRound& round, const ViewData& view, PrLayoutNode made) {
    for (uint32_t i = 0; i < view.others.count; ++i)
        jsKitDeclare(round, view.others.data[i], made);
}

PrLayoutNode listViewDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    ViewData& view = jsKitData<ViewData>(round, node);
    viewRefresh(round, node, view, listRead);
    PrKitListView list{.key = jsKitNodeKey(round, node), .count = view.rows.count, .selection = viewCursor(view)};
    PrKitListViewStyle style{
        .rowHeight = view.rowHeight,
        .tileWidth = view.tileWidth,
        .place = jsKitNodePlace(round.tree, node),
    };
    PrLayoutNode made = prKitListView(round.kit, parent, list, style);
    PrKitWindow window = prKitListWindow(round.kit, made);
    PR_ASSERT(window.first + window.count <= view.rows.count);
    for (uint32_t index = window.first; index < window.first + window.count; ++index) {
        JsKitNodeIndex item = view.rows.data[index];
        bool selected = rowOf(round.tree, item).selected;
        PrLayoutNode shown = kPrLayoutNodeNone;
        if (isKind(round.tree, item, Kind_ListTile))
            shown = prKitListTile(round.kit, made, {.index = index, .item = item, .selected = selected});
        else
            shown = prKitListRow(round.kit, made, {.index = index, .item = item, .selected = selected});
        rowClick(round, item, index, shown);
        jsKitDeclareChildren(round, item, shown);
    }
    viewSelect(round, node, view, made);
    viewDeclareOthers(round, view, made);
    return kPrLayoutNodeNone;
}

// Tells the script of the fold that the round asked of the tree view whose widget is `made`, if it asked for one. A
// view that the script does not listen to opens or closes the row instead.
void treeFold(const JsKitRound& round, JsKitNodeIndex node, ViewData& view, PrLayoutNode made) {
    PrKitTreeFold fold{};
    if (!prKitFolded(round.kit, made, &fold) || fold.row >= view.rows.count)
        return;
    if (jsKitNodeListens(round.tree, node, TreeEvent_Fold)) {
        JsKitDetail detail{.values = {double(fold.row), fold.open ? 1.0 : 0.0}, .valueCount = 2};
        jsKitEventPush(round, node, TreeEvent_Fold, &detail);
        return;
    }
    rowOf(round.tree, view.rows.data[fold.row]).open = fold.open;
    view.read = false;
}

// Tells the script of the row that the round dropped on a row of the tree view whose widget is `made`, if it dropped
// one.
void treeDrop(const JsKitRound& round, JsKitNodeIndex node, const ViewData& view, PrLayoutNode made) {
    PrKitTreeDrop drop{};
    if (!prKitRowDropped(round.kit, made, &drop))
        return;
    // A row's payload carries its node. The payload of another source is not among the view's rows.
    double dragged = -1;
    for (uint32_t i = 0; i < view.rows.count; ++i) {
        if (view.rows.data[i] == drop.item) {
            dragged = double(i);
            break;
        }
    }
    JsKitDetail detail{
        .values = {double(drop.row), dragged, double(drop.item)},
        .valueCount = 3,
        .hasText = true,
        .text = jsKitKeywordName(kDropZones, drop.zone),
    };
    jsKitEventPush(round, node, TreeEvent_Drop, &detail);
}

PrLayoutNode treeViewDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    ViewData& view = jsKitData<ViewData>(round, node);
    viewRefresh(round, node, view, treeRead);
    PrKitTreeView tree{
        .key = jsKitNodeKey(round, node),
        .count = view.rows.count,
        .selection = viewCursor(view),
        .items = view.items.data,
        .dragKind = view.dragKind,
    };
    PrKitListViewStyle style{.rowHeight = view.rowHeight, .place = jsKitNodePlace(round.tree, node)};
    PrLayoutNode made = prKitTreeView(round.kit, parent, tree, style);
    PrKitWindow window = prKitListWindow(round.kit, made);
    PR_ASSERT(window.first + window.count <= view.rows.count);
    for (uint32_t index = window.first; index < window.first + window.count; ++index) {
        JsKitNodeIndex item = view.rows.data[index];
        PrKitRow row{.index = index, .item = item, .selected = rowOf(round.tree, item).selected};
        PrLayoutNode shown = prKitTreeRow(round.kit, made, row);
        rowClick(round, item, index, shown);
        jsKitDeclareChildren(round, item, shown);
    }
    viewSelect(round, node, view, made);
    treeFold(round, node, view, made);
    treeDrop(round, node, view, made);
    viewDeclareOthers(round, view, made);
    return kPrLayoutNodeNone;
}

// A table's cell. `column` is the cell's column if `hasColumn` is true. The cell is otherwise in the column of its
// place among its row's cells.
struct CellData {
    uint32_t column;
    bool hasColumn;
    PrKitIconKind icon;
    PrKitTextRole role;
    PrKitTone tone;
};

bool cellSet(void* data, PrStringView name, PrStringView value, bool reset) {
    CellData& cell = *static_cast<CellData*>(data);
    if (name == "column") {
        if (!jsKitSetUint(value, reset, &cell.column))
            return false;
        cell.hasColumn = !reset;
        return true;
    }
    if (name == "icon")
        return jsKitSetIcon(value, reset, &cell.icon);
    if (name == "role")
        return jsKitSetKeyword(jsKitRoles(), value, reset, &cell.role);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &cell.tone);
    return false;
}

// Declares the cells of the table row whose widget is `made`, in a table of `columnCount` columns, and the row's other
// nodes after them.
void tableRowDeclareCells(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode made, uint32_t columnCount) {
    const JsKitNodeTree* tree = round.tree;
    uint32_t place = 0;
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (!isKind(tree, child, Kind_TableCell))
            continue;
        const CellData& cell = jsKitData<CellData>(round, child);
        uint32_t column = cell.hasColumn ? cell.column : place;
        ++place;
        if (column >= columnCount)
            continue;
        PrKitTableCell content{
            .column = column,
            .text = jsKitNodeText(tree, child),
            .icon = cell.icon,
            .role = cell.role,
            .tone = cell.tone,
        };
        jsKitDeclareChildren(round, child, prKitTableCell(round.kit, made, content));
    }
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (!isKind(tree, child, Kind_TableCell))
            jsKitDeclare(round, child, made);
    }
}

PrLayoutNode tableDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    ViewData& view = jsKitData<ViewData>(round, node);
    viewRefresh(round, node, view, tableRead);
    // A table with no columns still names where they would be.
    static constexpr PrKitTableColumn kNoColumns[1] = {};
    PrKitTable table{
        .key = jsKitNodeKey(round, node),
        .count = view.rows.count,
        .selection = viewCursor(view),
        .columns = view.columns.count != 0 ? view.columns.data : kNoColumns,
        .columnCount = view.columns.count,
        .sort = &view.sort,
    };
    PrKitListViewStyle style{.rowHeight = view.rowHeight, .place = jsKitNodePlace(round.tree, node)};
    PrLayoutNode made = prKitTable(round.kit, parent, table, style);
    if (prKitSorted(round.kit, made)) {
        JsKitDetail detail{.values = {double(view.sort.column), view.sort.descending ? 1.0 : 0.0}, .valueCount = 2};
        jsKitEventPush(round, node, TableEvent_Sort, &detail);
    }
    PrKitWindow window = prKitListWindow(round.kit, made);
    PR_ASSERT(window.first + window.count <= view.rows.count);
    for (uint32_t index = window.first; index < window.first + window.count; ++index) {
        JsKitNodeIndex item = view.rows.data[index];
        PrKitTableRow row{.index = index, .item = item, .selected = rowOf(round.tree, item).selected};
        PrLayoutNode shown = prKitTableRow(round.kit, made, row);
        rowClick(round, item, index, shown);
        tableRowDeclareCells(round, item, shown, view.columns.count);
    }
    viewSelect(round, node, view, made);
    viewDeclareOthers(round, view, made);
    return kPrLayoutNodeNone;
}

// ---------------------------------------------------------------------------------------------------------------------
// Parts

// A tile's picture, which the node owns a reference to, and the fraction by which it grows under the pointer.
struct TileImageData {
    const PrDrawImage* PR_NULLABLE image;
    float hoverZoom;
};

void tileImageDestroy(void* data) {
    prDrawImageRelease(static_cast<TileImageData*>(data)->image);
}

// Reads a number of pixels along a side of a picture, from one up to kImageMaxSide.
bool imageSideFrom(PrStringView text, uint32_t* out) {
    return jsKitSetUint(text, false, out) && *out >= 1 && *out <= kImageMaxSide;
}

// Creates the picture that `text` gives: its width, its height, and its pixels in base64, separated by single spaces.
// The pixels are RGBA8 with straight alpha in sRGB, row by row, and exactly as many as the extent holds. Returns false
// if `text` is anything else.
bool imageFrom(PrStringView text, const PrDrawImage* PR_NULLABLE* PR_NONNULL out) {
    size_t breaks[2] = {};
    uint32_t found = 0;
    for (size_t at = 0; at < text.size && found < 2; ++at) {
        if (text.data[at] == ' ')
            breaks[found++] = at;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    if (found < 2 || !imageSideFrom(PrStringView{text.data, breaks[0]}, &width) ||
            !imageSideFrom(PrStringView{text.data + breaks[0] + 1, breaks[1] - breaks[0] - 1}, &height))
        return false;
    PrStringView encoded{text.data + breaks[1] + 1, text.size - breaks[1] - 1};
    size_t size = size_t(width) * height * 4;
    size_t decoded = 0;
    if (!prBase64Decode(encoded, nullptr, 0, &decoded) || decoded != size)
        return false;
    uint8_t* pixels = static_cast<uint8_t*>(prMemoryAlloc(size));
    prBase64Decode(encoded, reinterpret_cast<char*>(pixels), size, &decoded);
    PrDrawImageDesc desc{
        .pixels = {pixels, size_t(width) * 4, {width, height}},
        .format = PrPixelFormat_Rgba8,
        .alpha = PrDrawAlpha_Straight,
        .space = PrColorSpace_Srgb,
    };
    PrStatus status = prDrawImageCreate(&desc, out);
    prMemoryFree(pixels);
    return status == PrStatus_Ok;
}

bool tileImageSet(void* data, PrStringView name, PrStringView value, bool reset) {
    TileImageData& tile = *static_cast<TileImageData*>(data);
    if (name == "hover-zoom")
        return jsKitSetLength(value, reset, &tile.hoverZoom);
    if (name == "image") {
        const PrDrawImage* image = nullptr;
        if (!reset && !imageFrom(value, &image))
            return false;
        prDrawImageRelease(tile.image);
        tile.image = image;
        return true;
    }
    return false;
}

PrLayoutNode tileImageDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const TileImageData& tile = jsKitData<TileImageData>(round, node);
    if (tile.image == nullptr)
        return kPrLayoutNodeNone;
    PrKitImage image{.key = jsKitNodeKey(round, node), .image = tile.image};
    return prKitTileImage(round.kit, parent, image, {.hoverZoom = tile.hoverZoom});
}

// When a visibility toggle is revealed.
enum {
    Reveal_Never,
    Reveal_Always,
    Reveal_Hover,                   // while the pointer is over the widget that the toggle is under
};
typedef uint8_t Reveal;

struct ToggleData {
    bool hidden;
    Reveal revealed;
};

bool toggleSet(void* data, PrStringView name, PrStringView value, bool reset) {
    ToggleData& toggle = *static_cast<ToggleData*>(data);
    if (name == "hidden")
        return jsKitSetFlag(value, reset, &toggle.hidden);
    if (name == "revealed") {
        if (!reset && value == "hover") {
            toggle.revealed = Reveal_Hover;
            return true;
        }
        bool revealed = false;
        if (!jsKitSetFlag(value, reset, &revealed))
            return false;
        toggle.revealed = revealed ? Reveal_Always : Reveal_Never;
        return true;
    }
    return false;
}

PrLayoutNode toggleDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const ToggleData& toggle = jsKitData<ToggleData>(round, node);
    bool revealed = toggle.revealed == Reveal_Always;
    if (toggle.revealed == Reveal_Hover)
        revealed = prUiIsHovered(round.kit->ui, parent);
    PrKitVisibilityToggle content{.key = jsKitNodeKey(round, node), .hidden = toggle.hidden, .revealed = revealed};
    PrLayoutNode made = prKitVisibilityToggle(round.kit, parent, content);
    if (prKitActivated(round.kit, made))
        jsKitEventPush(round, node, PartEvent_Click, nullptr);
    return made;
}

// The kind of payload that a drop zone takes, and where the payload that is over its widget would land, as the script
// last heard.
struct DropZoneData {
    uint32_t dragKind;
    PrKitDropZone over;
};

bool dropZoneSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "drag-kind")
        return jsKitSetUint(value, reset, &static_cast<DropZoneData*>(data)->dragKind);
    return false;
}

PrLayoutNode dropZoneDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    DropZoneData& zone = jsKitData<DropZoneData>(round, node);
    PrUi* ui = round.kit->ui;
    PrKitDropZone over = PrKitDropZone_None;
    if (zone.dragKind != 0 && prUiIsDropTarget(ui, parent, zone.dragKind))
        over = prKitDropZone(round.kit, parent);
    const PrUiPayload* dropped = zone.dragKind != 0 ? prUiDropped(ui, parent) : nullptr;
    if (dropped != nullptr && dropped->kind == zone.dragKind) {
        // A drop whose place the last layout cannot tell lands inside, as a drop on a tree's row does.
        PrKitDropZone landed = prKitDropZone(round.kit, parent);
        if (landed == PrKitDropZone_None)
            landed = PrKitDropZone_Inside;
        JsKitDetail detail{
            .values = {double(dropped->value)},
            .valueCount = 1,
            .hasText = true,
            .text = jsKitKeywordName(kDropZones, landed),
        };
        jsKitEventPush(round, node, DropEvent_Drop, &detail);
        over = PrKitDropZone_None;
    }
    if (over != zone.over) {
        zone.over = over;
        JsKitDetail detail{.hasText = true, .text = jsKitKeywordName(kDropZones, over)};
        jsKitEventPush(round, node, DropEvent_Dragover, &detail);
    }
    return kPrLayoutNodeNone;
}

// The family's kinds, each at the index that Kind gives it.
constexpr JsKitKind kKinds[] = {
    {
        .name = "list-view",
        .dataSize = sizeof(ViewData),
        .events = kListEvents,
        .eventCount = countOf(kListEvents),
        .flags = JsKitKindFlag_PlacesItself,
        .destroy = viewDestroy,
        .set = listViewSet,
        .declare = listViewDeclare,
    },
    {
        .name = "list-row",
        .dataSize = sizeof(RowData),
        .events = kClickEvents,
        .eventCount = countOf(kClickEvents),
        .set = rowSet,
        .declare = declareNothing,
    },
    {
        .name = "list-tile",
        .dataSize = sizeof(RowData),
        .events = kClickEvents,
        .eventCount = countOf(kClickEvents),
        .set = rowSet,
        .declare = declareNothing,
    },
    {
        .name = "tile-image",
        .dataSize = sizeof(TileImageData),
        .destroy = tileImageDestroy,
        .set = tileImageSet,
        .declare = tileImageDeclare,
    },
    {
        .name = "tree-view",
        .dataSize = sizeof(ViewData),
        .events = kTreeEvents,
        .eventCount = countOf(kTreeEvents),
        .flags = JsKitKindFlag_PlacesItself,
        .destroy = viewDestroy,
        .set = treeViewSet,
        .declare = treeViewDeclare,
    },
    {
        .name = "tree-row",
        .dataSize = sizeof(RowData),
        .events = kClickEvents,
        .eventCount = countOf(kClickEvents),
        .set = treeRowSet,
        .declare = declareNothing,
    },
    {
        .name = "visibility-toggle",
        .dataSize = sizeof(ToggleData),
        .events = kClickEvents,
        .eventCount = countOf(kClickEvents),
        .set = toggleSet,
        .declare = toggleDeclare,
    },
    {
        .name = "drop-zone",
        .dataSize = sizeof(DropZoneData),
        .events = kDropEvents,
        .eventCount = countOf(kDropEvents),
        .set = dropZoneSet,
        .declare = dropZoneDeclare,
    },
    {
        .name = "table",
        .dataSize = sizeof(ViewData),
        .events = kTableEvents,
        .eventCount = countOf(kTableEvents),
        .flags = JsKitKindFlag_PlacesItself,
        .create = tableCreate,
        .destroy = viewDestroy,
        .set = tableSet,
        .declare = tableDeclare,
    },
    {.name = "table-column", .dataSize = sizeof(ColumnData), .set = columnSet, .declare = declareNothing},
    {
        .name = "table-row",
        .dataSize = sizeof(RowData),
        .events = kClickEvents,
        .eventCount = countOf(kClickEvents),
        .set = rowSet,
        .declare = declareNothing,
    },
    {.name = "table-cell", .dataSize = sizeof(CellData), .set = cellSet, .declare = declareNothing},
};
static_assert(countOf(kKinds) == Kind_Count);

bool isKind(const JsKitNodeTree* tree, JsKitNodeIndex node, Kind kind) {
    return jsKitNodeKind(tree, node) == &kKinds[kind];
}

} // namespace

Span<const JsKitKind> jsKitCollectionKinds() {
    return kKinds;
}

} // namespace pr_impl
