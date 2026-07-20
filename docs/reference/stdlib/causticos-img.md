# std/causticos/img -- CSIF image decoding

`std/causticos/img/csif.cst` is the strict Caustic Standard Image Format
(CSIF) decoder used by CausticOS applications such as the compositor and
launcher. It validates untrusted container data before allocating memory or
dispatching to a codec.

The normative format and reference encoder live in the sibling
[causticos repository](https://github.com/Caua726/causticos):

- [CSIF format specification](https://github.com/Caua726/causticos/blob/main/docs/CSIF_FORMAT.md)
- [reference PNG/PPM encoder](https://github.com/Caua726/causticos/blob/main/scripts/png2csif.py)

## Supported profile

This module deliberately implements a closed BASELINE level-1 subset:

- container version 1, little-endian
- random-access layout
- CRC32-IEEE header and chunk integrity
- one implicit primary image
- mandatory `ILIM`, `IHDR`, `ICOL`, `ICOD`, `IDAT`, and `IEND` chunks
- flat 8-bit sRGB RGB or RGBA pixels
- one untiled, whole-image stream
- RAW codec 0 or QOI codec 1

Other layouts, profiles, colour encodings, codecs, tiling, sub-images, and
progressive streams fail with a numbered error. They are never partially
decoded.

## Image

```cst
struct Image {
    ok          as i64;
    err         as i64;
    width       as i64;
    height      as i64;
    nchan       as i64;
    pixels      as *u8;
    pix_map_len as i64;
}
```

When `ok == 1`, `pixels` contains `width * height * nchan` interleaved bytes.
`nchan` is 3 for RGB or 4 for RGBA. When `ok == 0`, `err` contains a numbered
container, codec, allocation, or I/O error and `pixels` is null.

## load

```cst
fn load(path as *u8) as Image
```

Read and decode one file through the CausticOS VFS:

```cst
use "std/causticos/img/csif.cst" as csif;

let is csif.Image as image = csif.load(cast(*u8, "/etc/wallpaper.csif"));
if (image.ok == 1) {
    // consume image.pixels
    csif.free(&image);
}
```

`load` is CausticOS-specific and must be built for `--target=caustic-x86_64`.
The file buffer is released before the function returns; decoded pixels remain
valid until `free`.

## decode_into

```cst
fn decode_into(buf as *u8, len as i64, out as *u8, out_cap as i64) as Image
```

Validate and decode into caller-owned storage. This path performs no allocation
or syscalls and is suitable for conformance tests on other targets. On success,
`pixels == out` and `pix_map_len == 0`; the caller retains ownership and does not
call `free`.

The input buffer must remain valid for the duration of the call. `out_cap` must
hold the full decoded image or the function returns `ERR_OUTPUT`.

## parse

```cst
fn parse(buf as *u8, len as i64) as Parsed
```

Low-level, allocation-free container validation. A successful `Parsed` contains
dimensions and a codec-byte pointer into `buf`; therefore the input buffer must
outlive the returned metadata. Most callers should use `decode_into` or `load`.

## free

```cst
fn free(image as *Image) as void
```

Release a pixel mapping owned by `load` or `decode_bytes`. The function clears
the pixel pointer and ownership length. Caller-owned `decode_into` buffers are
not mappings and remain the caller's responsibility.

## Validation and errors

Before any pixel allocation, the parser checks:

- bounded header and directory ranges without overflowing additions
- sorted, aligned, non-overlapping chunk ranges
- directory/header type and flag agreement
- canonical chunk ordering and mandatory singleton presence
- header and per-chunk CRC32
- `ILIM` resource ceilings against decoder hard caps
- dimension and byte-size products before multiplication/allocation
- BASELINE geometry, colour, codec parameters, and `IDAT` coordinates

Container errors `1..25` match the CSIF specification's section 1.5 numbering.
API errors `26..30` cover open, read, allocation, codec decode, and output-buffer
failures.

`codec_raw.cst` and `codec_qoi.cst` are syscall-free and can be imported
independently. They reject invalid dimensions, truncated streams, overruns, and
non-canonical trailing bytes.
