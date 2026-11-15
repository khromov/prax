// Studio Kit's sizes (pr/kit_theme.h), by the names of its constants: kPrKitSpace4 is space[4], and kPrKitHeightSm
// is height.sm. A component gives a size as an attribute in px, so these keep the gallery's numbers the kit's.

export const space = { 1: 2, 2: 4, 3: 6, 4: 8, 5: 10, 6: 12, 8: 16, 10: 20, 12: 24, 16: 32 };

export const height = { xxs: 14, xs: 18, sm: 22, md: 26, lg: 30, xl: 36 };

export const radius = 3;

// The text styles that the gallery's sections share (examples/studio_gallery/gallery_parts.h), each as the attributes
// of a label, which a component spreads: <label {...text.caption}>.
export const text = {
    bodySecondary: { tone: 'secondary' },
    lede: { tone: 'muted', 'text-align': 'end' },
    strong: { role: 'strong' },
    title: { role: 'title' },
    caption: { role: 'caption' },
    captionAccent: { role: 'caption', tone: 'accent' },
    captionFaint: { role: 'caption', tone: 'faint' },
    meta: { role: 'meta' },
    metaAccent: { role: 'meta', tone: 'accent' },
    metaMuted: { role: 'meta', tone: 'muted' },
    mono: { role: 'mono' },
    monoSecondary: { role: 'mono', tone: 'secondary' },
    monoMuted: { role: 'mono', tone: 'muted' },
    monoFaint: { role: 'mono', tone: 'faint' },
    monoStrong: { role: 'mono-strong' },
    monoStrongFaint: { role: 'mono-strong', tone: 'faint' },
};
