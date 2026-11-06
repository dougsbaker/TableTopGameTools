"""Turns the dice line drawings in dice-icons.png into LVGL A8 images (main/apps/dice/dice_icons.cpp).

The source sheet holds one light-gray tile per die, with black line art (d6, d4, d12, d10, d8 / d20).
Each drawing becomes an alpha-only image, so the firmware can draw it in any color. All six are scaled
by the same factor, so their line weights match.

Standard library only:
    python make_dice_icons.py dice-icons.png ../../main/apps/dice/dice_icons.cpp [--preview out.png]
"""
import argparse
import math
import struct
import zlib

ICON_SIZE = 84  # square canvas, px
ART_SIZE = 82  # the largest drawing's longer side fills this much of the canvas
SHEET_ORDER = [6, 4, 12, 10, 8, 20]  # dice in the sheet, left to right then top to bottom
SIDES = [4, 6, 8, 10, 12, 20]  # firmware order (dice::kSides)


def read_png(path):
    """8-bit RGB or RGBA, non-interlaced. Returns (width, height, rows of (r, g, b) luminance 0-255)."""
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a PNG: %s" % path)
    pos, idat = 8, []
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color_type not in (2, 6) or interlace:
                raise SystemExit("unsupported PNG (depth %d, type %d, interlace %d)" % (depth, color_type, interlace))
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
        pos += 12 + length
    bpp = 4 if color_type == 6 else 3
    raw = zlib.decompress(b"".join(idat))
    stride = width * bpp
    prev = bytearray(stride)
    lum = []
    i = 0
    for _ in range(height):
        filt = raw[i]
        line = bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        if filt == 1:
            for x in range(bpp, stride):
                line[x] = (line[x] + line[x - bpp]) & 0xFF
        elif filt == 2:
            for x in range(stride):
                line[x] = (line[x] + prev[x]) & 0xFF
        elif filt == 3:
            for x in range(stride):
                left = line[x - bpp] if x >= bpp else 0
                line[x] = (line[x] + ((left + prev[x]) >> 1)) & 0xFF
        elif filt == 4:
            for x in range(stride):
                a = line[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[x] = (line[x] + pred) & 0xFF
        row = []
        for x in range(0, stride, bpp):
            r, g, b = line[x], line[x + 1], line[x + 2]
            alpha = line[x + 3] if bpp == 4 else 255
            y = 0.2126 * r + 0.7152 * g + 0.0722 * b
            row.append(y * alpha / 255 + 255 * (255 - alpha) / 255)  # over white
        lum.append(row)
        prev = line
    return width, height, lum


def write_png(path, width, height, rgb_rows):
    raw = b"".join(b"\x00" + bytes(v for px in row for v in px) for row in rgb_rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def runs(flags, min_len):
    """[start, end) spans where flags is true for at least min_len entries in a row."""
    spans, start = [], None
    for i, flag in enumerate(flags + [False]):
        if flag and start is None:
            start = i
        elif not flag and start is not None:
            if i - start >= min_len:
                spans.append((start, i))
            start = None
    return spans


def find_tiles(width, height, lum):
    """The gray tiles, left to right then top to bottom, as (x0, y0, x1, y1)."""
    tinted = [[v < 252 for v in row] for row in lum]
    bands = runs([sum(row) > 100 for row in tinted], 100)
    tiles = []
    for y0, y1 in bands:
        cols = [any(tinted[y][x] for y in range(y0, y1)) for x in range(width)]
        tiles += [(x0, y0, x1, y1) for x0, x1 in runs(cols, 100)]
    return tiles


def coverage(lum, tile):
    """Ink coverage 0-1 for every pixel of a tile: tile gray = 0, black = 1."""
    x0, y0, x1, y1 = tile
    inset = 6
    border = [lum[y][x] for y in (y0 + inset, y1 - 1 - inset) for x in range(x0 + inset, x1 - inset)]
    paper = sorted(border)[len(border) // 2]
    return [[min(1.0, max(0.0, (paper - lum[y][x]) / paper)) for x in range(x0, x1)] for y in range(y0, y1)]


def bbox(cov, threshold=0.25):
    ys = [y for y, row in enumerate(cov) if max(row) > threshold]
    xs = [x for x in range(len(cov[0])) if any(row[x] > threshold for row in cov)]
    return xs[0], ys[0], xs[-1] + 1, ys[-1] + 1


def area_weights(origin, step, count, limit):
    """For each output pixel, the (source index, weight) pairs of a box filter over [origin + i*step, +step)."""
    out = []
    for i in range(count):
        a, b = origin + i * step, origin + (i + 1) * step
        taps = []
        for j in range(math.floor(a), math.ceil(b)):
            overlap = min(b, j + 1) - max(a, j)
            if overlap > 0 and 0 <= j < limit:
                taps.append((j, overlap / step))
        out.append(taps)
    return out


def render(cov, box, scale):
    """Resample the tile's coverage so the drawing's box centre lands in the middle of the canvas."""
    bx0, by0, bx1, by1 = box
    step = 1 / scale
    ox = (bx0 + bx1) / 2 - ICON_SIZE * step / 2
    oy = (by0 + by1) / 2 - ICON_SIZE * step / 2
    wx = area_weights(ox, step, ICON_SIZE, len(cov[0]))
    wy = area_weights(oy, step, ICON_SIZE, len(cov))
    rows = {}
    for taps in wy:
        for j, _ in taps:
            if j not in rows:
                rows[j] = [sum(cov[j][k] * w for k, w in tx) for tx in wx]
    icon = []
    for taps in wy:
        icon.append([min(255, round(255 * sum(rows[j][x] * w for j, w in taps))) for x in range(ICON_SIZE)])
    return icon


def write_cpp(path, icons):
    lines = [
        "// Generated by tools/dice_icons/make_dice_icons.py from tools/dice_icons/dice-icons.png. Do not edit.",
        "#include \"apps/dice/dice_icons.h\"",
        "",
        "namespace dice {",
        "namespace {",
        "",
        "constexpr uint32_t kIconSize = %d;" % ICON_SIZE,
        "",
    ]
    for sides in SIDES:
        lines.append("constexpr uint8_t kD%d[kIconSize * kIconSize] = {" % sides)
        flat = [v for row in icons[sides] for v in row]
        for i in range(0, len(flat), 21):
            lines.append("    " + " ".join("0x%02x," % v for v in flat[i:i + 21]))
        lines.append("};")
        lines.append("")
    lines += [
        "constexpr lv_image_dsc_t icon(const uint8_t *data)",
        "{",
        "    lv_image_dsc_t dsc = {};",
        "    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;",
        "    dsc.header.cf = LV_COLOR_FORMAT_A8;",
        "    dsc.header.w = kIconSize;",
        "    dsc.header.h = kIconSize;",
        "    dsc.header.stride = kIconSize;",
        "    dsc.data_size = kIconSize * kIconSize;",
        "    dsc.data = data;",
        "    return dsc;",
        "}",
        "",
        "constexpr lv_image_dsc_t kIcons[] = {%s};" % ", ".join("icon(kD%d)" % s for s in SIDES),
        "",
        "}  // namespace",
        "",
        "const lv_image_dsc_t *die_icon(size_t type)",
        "{",
        "    return &kIcons[type < sizeof(kIcons) / sizeof(kIcons[0]) ? type : 0];",
        "}",
        "",
        "}  // namespace dice",
        "",
    ]
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def hexrgb(value):
    return ((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF)


def preview(path, icons):
    """A mock of the dice screen at 2x: the firmware's layout and colors, d6 and d20 selected."""
    bg, surface, text, violet = hexrgb(0x0B0E14), hexrgb(0x161B24), hexrgb(0xF5F7FA), hexrgb(0x7C3AED)
    colors = {4: 0xDC2626, 6: 0x2563EB, 8: 0x16A34A, 10: 0xD97706, 12: 0x0891B2, 20: 0xDB2777}
    selected = {6, 20}
    tile, orbit, cx, cy, roll = 100, 158, 240, 242, 170
    img = [[bg] * 480 for _ in range(480)]
    for y in range(50):
        img[y] = [surface] * 480
    for y in range(480):
        for x in range(480):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= (roll / 2) ** 2:
                img[y][x] = violet
    for i, sides in enumerate(SIDES):
        angle = 2 * math.pi * i / 6 - 2 * math.pi / 3
        tx = cx + round(orbit * math.cos(angle)) - tile // 2
        ty = cy + round(orbit * math.sin(angle)) - tile // 2
        fill = hexrgb(colors[sides]) if sides in selected else surface
        ink = text if sides in selected else hexrgb(colors[sides])
        for y in range(ty, ty + tile):
            for x in range(tx, tx + tile):
                img[y][x] = fill
        off = (tile - ICON_SIZE) // 2
        for y in range(ICON_SIZE):
            for x in range(ICON_SIZE):
                a = icons[sides][y][x] / 255
                if a:
                    p = img[ty + off + y][tx + off + x]
                    img[ty + off + y][tx + off + x] = tuple(round(ink[k] * a + p[k] * (1 - a)) for k in range(3))
        if sides in selected:  # the "xN" badge's box
            shade = tuple(round(c * 0.41) for c in hexrgb(colors[sides]))
            for y in range(ty + 4, ty + 22):
                for x in range(tx + tile - 4 - 26, tx + tile - 4):
                    img[y][x] = shade
    big = []
    for row in img:
        wide = [px for px in row for _ in (0, 1)]
        big += [wide, wide]
    write_png(path, 960, 960, big)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("sheet")
    parser.add_argument("out_cpp")
    parser.add_argument("--preview")
    args = parser.parse_args()

    width, height, lum = read_png(args.sheet)
    tiles = find_tiles(width, height, lum)
    if len(tiles) != len(SHEET_ORDER):
        raise SystemExit("found %d tiles, expected %d: %s" % (len(tiles), len(SHEET_ORDER), tiles))
    covs, boxes = {}, {}
    for sides, tile in zip(SHEET_ORDER, tiles):
        covs[sides] = coverage(lum, tile)
        boxes[sides] = bbox(covs[sides])
        x0, y0, x1, y1 = boxes[sides]
        print("d%-2d tile %s  drawing %dx%d" % (sides, tile, x1 - x0, y1 - y0))
    longest = max(max(b[2] - b[0], b[3] - b[1]) for b in boxes.values())
    scale = ART_SIZE / longest
    print("scale %.3f (longest side %d px)" % (scale, longest))
    icons = {sides: render(covs[sides], boxes[sides], scale) for sides in SIDES}
    write_cpp(args.out_cpp, icons)
    print("wrote", args.out_cpp)
    if args.preview:
        preview(args.preview, icons)
        print("wrote", args.preview)


if __name__ == "__main__":
    main()
