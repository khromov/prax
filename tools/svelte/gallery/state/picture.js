// The picture that the gallery's images section fits into its frames (examples/studio_gallery/gallery_state.cpp
// galleryPicture): a landscape twice as wide as it is tall, with a sun toward its right, so that each crop shows a
// different part of it.
//
// The example computes the picture in floats, and so does this: each step is rounded to a float as the example's is,
// so that the two hold the same bytes. The pixels are in sRGB with straight alpha, which is what the example gives
// the kit and what an image's `pixels` takes.

const width = 512;
const height = 256;

const float = Math.fround;

// Returns the value `t` of the way from `a` to `b`, as floats.
function mix(a, b, t) {
    return float(float(a) + float(float(float(b) - float(a)) * t));
}

// Returns a ridge's height at `u` across the picture: `base` and two waves, each a height and the sine of `u` at a
// rate and a phase.
function ridge(u, base, first, second) {
    const one = float(float(first.height) * float(Math.sin(float(float(u * first.rate) + first.phase))));
    const two = float(float(second.height) * float(Math.sin(float(float(u * second.rate) + second.phase))));
    return float(float(float(base) + one) + two);
}

// Returns a channel as a byte.
function byte(channel) {
    return Math.trunc(float(float(float(channel) * 255) + 0.5));
}

// Returns the picture's pixels: rows from the top, of four bytes for each pixel.
function pixels() {
    const out = new Uint8Array(width * height * 4);

    // A ridge's height depends on the column alone.
    const far = new Float32Array(width);
    const near = new Float32Array(width);
    for (let x = 0; x < width; ++x) {
        const u = float((x + 0.5) / width);
        far[x] = ridge(u, 0.62, { height: 0.08, rate: 9, phase: 1 }, { height: 0.04, rate: 23, phase: 0 });
        near[x] = ridge(u, 0.76, { height: 0.07, rate: 5, phase: 3 }, { height: 0.03, rate: 17, phase: 2 });
    }
    for (let y = 0; y < height; ++y) {
        for (let x = 0; x < width; ++x) {
            const u = float((x + 0.5) / width);
            const v = float((y + 0.5) / height);
            // A sky from deep blue to a warm horizon, a sun toward the right, and two ridges of hills before it.
            const t = float(v / float(0.7));
            let color = [mix(0.1, 0.95, t), mix(0.16, 0.55, t), mix(0.38, 0.32, t)];
            const dx = float(float(u - float(0.72)) * 2);
            const dy = float(v - float(0.45));
            if (float(float(dx * dx) + float(dy * dy)) < float(0.012))
                color = [1.0, 0.86, 0.55];
            if (v > far[x])
                color = [0.22, 0.24, 0.36];
            if (v > near[x])
                color = [0.1, 0.16, 0.14];
            const texel = (y * width + x) * 4;
            for (let c = 0; c < 3; ++c)
                out[texel + c] = byte(color[c]);
            out[texel + 3] = 255;
        }
    }
    return out;
}

const DIGITS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

// Returns the base64 of `bytes` (RFC 4648), padded.
function base64(bytes) {
    // The text is made a part at a time, since a string that grows by a few characters at a time is slow to make.
    const parts = [];
    const step = 3 * 1024;
    for (let start = 0; start < bytes.length; start += step) {
        const end = Math.min(start + step, bytes.length);
        let part = '';
        for (let at = start; at < end; at += 3) {
            const held = end - at;
            const group = (bytes[at] << 16) | ((held > 1 ? bytes[at + 1] : 0) << 8) | (held > 2 ? bytes[at + 2] : 0);
            part += DIGITS[group >> 18] + DIGITS[(group >> 12) & 63];
            part += held > 1 ? DIGITS[(group >> 6) & 63] : '=';
            part += held > 2 ? DIGITS[group & 63] : '=';
        }
        parts.push(part);
    }
    return parts.join('');
}

let picture = null;

// Returns the picture as the value of an image's `pixels`: its width, its height and the base64 of its pixels. It is
// made on first use.
export function galleryPicture() {
    picture ??= width + ' ' + height + ' ' + base64(pixels());
    return picture;
}
