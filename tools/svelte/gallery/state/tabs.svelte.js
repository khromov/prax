// The gallery's tab strip and segmented controls (gallery_state TabsState). The strip's node holds its tabs, which are
// five panels by their numbers plus one, and their order. `panel` is the tab that the strip last said it selected.

export const tabs = $state({
    panel: 1,
    viewMode: 1,
    shape: 0,
});
