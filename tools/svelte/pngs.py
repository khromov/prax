"""PNG files as the hosts write them, read and compared: `python3 -m tools.svelte.pngs <a.png> <b.png> [diff.png]`
prints how the two differ, and writes an image that marks where."""

import struct
import sys
import zlib


def read(path):
    """Returns the width, the height and the RGBA bytes of the PNG at `path`, which is 8 bits a channel, RGB or RGBA,
    and not interlaced, as the engine's writer makes them."""
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    at = 8
    packed = b""
    width = height = channels = 0
    while at < len(data):
        size, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + size]
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and color in (2, 6) and interlace == 0, path
            channels = 4 if color == 6 else 3
        elif kind == b"IDAT":
            packed += body
        at += 12 + size
    raw = zlib.decompress(packed)
    stride = width * channels
    rows = []
    previous = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        filtered = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        if filtered == 1:
            for i in range(channels, stride):
                row[i] = (row[i] + row[i - channels]) & 255
        elif filtered == 2:
            for i in range(stride):
                row[i] = (row[i] + previous[i]) & 255
        elif filtered == 3:
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                row[i] = (row[i] + (left + previous[i]) // 2) & 255
        elif filtered == 4:
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                above = previous[i]
                corner = previous[i - channels] if i >= channels else 0
                estimate = left + above - corner
                nearest = min((abs(estimate - left), 0, left), (abs(estimate - above), 1, above),
                              (abs(estimate - corner), 2, corner))[2]
                row[i] = (row[i] + nearest) & 255
        rows.append(row)
        previous = row
    if channels == 4:
        return width, height, b"".join(bytes(row) for row in rows)
    rgba = bytearray()
    for row in rows:
        for x in range(width):
            rgba += row[x * 3:x * 3 + 3] + b"\xff"
    return width, height, bytes(rgba)


def write(path, width, height, rgba):
    """Writes RGBA bytes as a PNG at `path`."""
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    rows = b"".join(b"\x00" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def differences(first, second, marked=None):
    """Compares two PNGs. Returns None if they hold the same pixels, and otherwise what differs: the number of pixels,
    and the box that holds them all, as (count, left, top, right, bottom), or a string if their sizes differ. Writes
    the first image to `marked`, if a path is given, dimmed, with each differing pixel in red."""
    a_width, a_height, a = read(first)
    b_width, b_height, b = read(second)
    if (a_width, a_height) != (b_width, b_height):
        return "%dx%d against %dx%d" % (a_width, a_height, b_width, b_height)
    if a == b:
        return None
    count = 0
    left, top, right, bottom = a_width, a_height, 0, 0
    out = bytearray(a)
    for y in range(a_height):
        start = y * a_width * 4
        if a[start:start + a_width * 4] == b[start:start + a_width * 4]:
            if marked:
                for i in range(start, start + a_width * 4, 4):
                    out[i], out[i + 1], out[i + 2] = out[i] // 3, out[i + 1] // 3, out[i + 2] // 3
            continue
        for x in range(a_width):
            i = start + x * 4
            if a[i:i + 4] != b[i:i + 4]:
                count += 1
                left, top, right, bottom = min(left, x), min(top, y), max(right, x + 1), max(bottom, y + 1)
                out[i:i + 4] = b"\xff\x00\x00\xff"
            elif marked:
                out[i], out[i + 1], out[i + 2] = out[i] // 3, out[i + 1] // 3, out[i + 2] // 3
    if marked:
        write(marked, a_width, a_height, bytes(out))
    return count, left, top, right, bottom


def describe(found):
    """Returns the sentence that says what `differences` found."""
    if found is None:
        return "the same pixels"
    if isinstance(found, str):
        return "sizes differ: " + found
    count, left, top, right, bottom = found
    return "%d pixels differ, within x %d to %d and y %d to %d" % (count, left, right, top, bottom)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit("usage: python3 -m tools.svelte.pngs <a.png> <b.png> [diff.png]")
    result = differences(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) == 4 else None)
    print(describe(result))
    sys.exit(0 if result is None else 1)
