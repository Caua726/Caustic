#!/usr/bin/env python3
"""gen-font.py — writes std/causticos/font.cst from a PSF console font.

    python3 tools/gen-font.py [FONT.psfu[.gz]]     (from the repository root)

The default is LatGrkCyr-8x16 from kbd's console fonts (on most Linux systems
at /usr/share/kbd/consolefonts or /usr/share/consolefonts). Its README says
"The font is public domain", which is why it is this font: the bitmaps end up
inside every CausticOS program that draws text, under the MIT licence the
rest of the code is under, and a GPL font cannot go there.

It covers Latin (Western and Central European, Turkish, Baltic, Vietnamese
base letters), Greek, Cyrillic, box drawing, block elements and the common
typographic punctuation — 512 glyphs for 644 code points, all in the BMP.

The PSF's own Unicode table decides which code point draws which glyph, so a
glyph shared by several code points (A and Greek capital alpha) is stored
once. A code point the font lacks draws U+FFFD's glyph.

The font has no ¬ (U+00AC) or ¦ (U+00A6), and keyboard layouts type both
(Brazilian ABNT2 and US-International have them on AltGr), so SUPPLEMENT
draws them, in the font's own one-pixel strokes: ¬ is its "-" with a tick
down at the right end, ¦ is its "|" broken in the middle. Drawn here, they are
under the same terms as the rest of this file. A supplement never replaces a
glyph the font has.
"""
import gzip
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DIRS = ["/usr/share/kbd/consolefonts", "/usr/share/consolefonts",
                "/usr/lib/kbd/consolefonts"]
DEFAULT_NAME = "LatGrkCyr-8x16.psfu.gz"


def load_psf(path):
    raw = open(path, "rb").read()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    if raw[:2] == b"\x36\x04":                       # PSF1
        mode, size = raw[2], raw[3]
        n = 512 if mode & 1 else 256
        if not mode & 2:
            sys.exit("gen-font: {} has no Unicode table".format(path))
        width, height, off = 8, size, 4
        glyphs = [raw[off + i * size:off + (i + 1) * size] for i in range(n)]
        table, cmap, g, i = raw[off + n * size:], {}, 0, 0
        while g < n and i + 1 < len(table):
            u = struct.unpack_from("<H", table, i)[0]
            i += 2
            if u == 0xFFFF:
                g += 1
            elif u != 0xFFFE:
                cmap.setdefault(u, g)
        return width, height, glyphs, cmap
    magic, _, hsize, flags, n, bpg, height, width = struct.unpack_from("<8I", raw, 0)
    if magic != 0x864AB572:
        sys.exit("gen-font: {} is not a PSF font".format(path))
    if not flags & 1:
        sys.exit("gen-font: {} has no Unicode table".format(path))
    glyphs = [raw[hsize + i * bpg:hsize + (i + 1) * bpg] for i in range(n)]
    table, cmap, g, i = raw[hsize + n * bpg:], {}, 0, 0
    while g < n and i < len(table):
        b = table[i]
        if b == 0xFF:
            g += 1
            i += 1
            continue
        if b == 0xFE:                                # a sequence follows: skip it
            i += 1
            continue
        ln = 1 if b < 0x80 else 2 if b < 0xE0 else 3 if b < 0xF0 else 4
        cmap.setdefault(ord(table[i:i + ln].decode("utf-8")), g)
        i += ln
    return width, height, glyphs, cmap


# Glyphs the font lacks, 16 rows each (MSB = leftmost pixel).
SUPPLEMENT = {
    0x00AC: [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       # ¬
             0x7E, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00],
    0x00A6: [0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00,       # ¦
             0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00],
}


def hexbytes(data):
    return "".join("\\x{:02x}".format(b) for b in data)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else None
    if path is None:
        for d in DEFAULT_DIRS:
            if os.path.exists(os.path.join(d, DEFAULT_NAME)):
                path = os.path.join(d, DEFAULT_NAME)
                break
    if path is None:
        sys.exit("gen-font: {} not found in {} — pass the font's path".format(
            DEFAULT_NAME, ", ".join(DEFAULT_DIRS)))
    width, height, glyphs, cmap = load_psf(path)
    if width != 8 or height != 16:
        sys.exit("gen-font: {} is {}x{}; the terminal draws 8x16 cells".format(path, width, height))
    added = 0
    for cp, rows in sorted(SUPPLEMENT.items()):
        if cp not in cmap:
            cmap[cp] = len(glyphs)
            glyphs.append(bytes(rows))
            added += 1
    if 0xFFFD not in cmap:
        sys.exit("gen-font: {} has no U+FFFD to draw what it lacks".format(path))
    if max(cmap) > 0xFFFF:
        sys.exit("gen-font: {} maps a code point past the BMP; the map holds u16".format(path))

    # Only glyphs some code point reaches are kept, renumbered densely.
    used = sorted(set(cmap.values()))
    renum = {g: i for i, g in enumerate(used)}
    entries = sorted((cp, renum[g]) for cp, g in cmap.items())
    gdata = b"".join(glyphs[g] for g in used)
    mdata = b"".join(struct.pack("<HH", cp, g) for cp, g in entries)

    name = os.path.basename(path)
    for ext in (".gz", ".psfu", ".psf"):
        if name.endswith(ext):
            name = name[:-len(ext)]
    out = []
    out.append("// std/causticos/font.cst — the 8x16 console font, with a Unicode map.\n")
    out.append("//\n")
    out.append("// GENERATED by tools/gen-font.py from {} (kbd's console fonts; its\n".format(name))
    out.append("// README: \"The font is public domain\"), with {} glyph(s) it lacks drawn in\n".format(added))
    out.append("// tools/gen-font.py's SUPPLEMENT. DO NOT EDIT. Regenerate from the\n")
    out.append("// repository root: python3 tools/gen-font.py [FONT.psfu]\n")
    out.append("//\n")
    out.append("// {} glyphs for {} code points: Latin, Greek, Cyrillic, box drawing, block\n".format(len(used), len(entries)))
    out.append("// elements, typographic punctuation. A glyph is 16 rows of 8 pixels, one byte\n")
    out.append("// a row, MSB = leftmost. The map is sorted by code point, 4 bytes an entry\n")
    out.append("// (code point u16 LE, glyph u16 LE). Both are read-only data: nothing to\n")
    out.append("// initialise, and one copy shared by every process mapping the same image.\n\n")
    out.append("let is i64 as GLYPH_W with imut = 8;\n")
    out.append("let is i64 as GLYPH_H with imut = 16;\n")
    out.append("let is i64 as N_GLYPHS with imut = {};\n".format(len(used)))
    out.append("let is i64 as N_MAP with imut = {};\n".format(len(entries)))
    out.append("// What a code point the font lacks draws: U+FFFD's glyph.\n")
    out.append("let is i64 as REPLACEMENT with imut = {};\n\n".format(renum[cmap[0xFFFD]]))
    out.append("let is *u8 as GLYPHS with imut = \"{}\";\n\n".format(hexbytes(gdata)))
    out.append("let is *u8 as MAP with imut = \"{}\";\n\n".format(hexbytes(mdata)))
    out.append("""// The glyph cp draws with, or -1 when the font has none.
fn glyph_index(cp as i64) as i64 {
    let is *u8 as m = MAP;
    let is i64 as lo with mut = 0;
    let is i64 as hi with mut = N_MAP - 1;
    while (lo <= hi) {
        let is i64 as mid = (lo + hi) / 2;
        let is i64 as c = cast(i64, m[mid * 4]) | (cast(i64, m[mid * 4 + 1]) << 8);
        if (cp < c) { hi = mid - 1; }
        else if (cp > c) { lo = mid + 1; }
        else { return cast(i64, m[mid * 4 + 2]) | (cast(i64, m[mid * 4 + 3]) << 8); }
    }
    return 0 - 1;
}

// 1 when the font draws cp itself rather than the replacement.
fn has_glyph(cp as i64) as i64 {
    if (glyph_index(cp) >= 0) { return 1; }
    return 0;
}

// The 16-byte bitmap cp is drawn with — U+FFFD's when the font lacks it.
fn glyph(cp as i64) as *u8 {
    let is i64 as g with mut = glyph_index(cp);
    if (g < 0) { g = REPLACEMENT; }
    return cast(*u8, cast(i64, GLYPHS) + g * 16);
}
""")
    dst = os.path.join(ROOT, "std", "causticos", "font.cst")
    with open(dst, "w") as fh:
        fh.write("".join(out))
    print("gen-font: {} glyphs, {} code points from {} -> std/causticos/font.cst".format(
        len(used), len(entries), name))


if __name__ == "__main__":
    main()
