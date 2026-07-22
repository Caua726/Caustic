// wasi_run.mjs — minimal pure-JS WASI (preview1) runner for Caustic .wasm modules.
//
// Usage:
//   node wasi_run.mjs <module.wasm> <preopen-dir> [guest-args...]
//
// Example — run a program compiled to wasm:
//   node wasi_run.mjs hello.wasm .
//
// Example — the compiler (built with `caustic --target=wasm32-wasi src/main.cst`)
// compiling a program, with the repo as the preopen root:
//   node wasi_run.mjs caustic.wasm /path/to/Caustic \
//        --target=wasm32-wasi /src/main.cst -o /caustic.self.wasm
//
// The compiler's .wasm also runs under node's built-in `node:wasi` (the memory is
// emitted `shared`, so memory.grow never detaches the buffer node:wasi caches).
// This tiny runner is just a dependency-free, easily-scriptable alternative that
// sandboxes fs to the preopen and is handy in CI.
//
// Implements exactly what the Caustic stdlib calls: proc_exit, fd_write, fd_read,
// fd_close, fd_seek, path_open (relative to a single preopen at fd 3),
// args_sizes_get, args_get. Paths are sandboxed to the preopen directory.

import { readFileSync, openSync, readSync, writeSync, closeSync, fstatSync,
         statSync, unlinkSync, mkdirSync, rmdirSync, renameSync } from 'node:fs';
import { resolve, normalize } from 'node:path';
import { randomFillSync } from 'node:crypto';
import { hrtime } from 'node:process';

const [modPath, preopen, ...guestArgs] = process.argv.slice(2);
if (!modPath || !preopen) {
  console.error('usage: node wasi_run.mjs <module.wasm> <preopen-dir> [guest-args...]');
  process.exit(2);
}
const ROOT = resolve(preopen);
const ARGS = ['caustic', ...guestArgs];

let mem, view, u8;
const refresh = () => { const b = mem.buffer; if (!view || view.buffer !== b) { view = new DataView(b); u8 = new Uint8Array(b); } };
const rd32 = o => (refresh(), view.getUint32(o, true));
const wr32 = (o, v) => (refresh(), view.setUint32(o, v >>> 0, true));
const wr64 = (o, v) => (refresh(), view.setBigUint64(o, BigInt(v), true));
const str  = (o, l) => (refresh(), Buffer.from(u8.subarray(o, o + l)).toString('utf8'));

// fd table: 0/1/2 = stdio, 3 = preopen dir, 4+ = opened files {rfd, off}.
const files = new Map();
let nextFd = 4;
const enc = new TextEncoder();

// Resolve a guest path against the preopen, refusing to escape it.
const relResolve = p => {
  const q = p.replace(/^\.?\//, '');            // guest strips ./ or / already; be safe
  const full = normalize(resolve(ROOT, q));
  if (full !== ROOT && !full.startsWith(ROOT + '/')) throw new Error('sandbox escape: ' + full);
  return full;
};

let exitCode = 0;
class ExitErr extends Error { constructor(c) { super('exit'); this.code = c; } }

const wasi = {
  proc_exit(code) { exitCode = code; throw new ExitErr(code); },
  fd_write(fd, iovs, n, nwritten) {
    refresh(); let total = 0;
    for (let i = 0; i < n; i++) {
      const p = rd32(iovs + i * 8), l = rd32(iovs + i * 8 + 4);
      const buf = Buffer.from(u8.subarray(p, p + l));
      if (fd === 1) { process.stdout.write(buf); total += l; }
      else if (fd === 2) { process.stderr.write(buf); total += l; }
      else { const f = files.get(fd); if (!f) return 8; const w = writeSync(f.rfd, buf, 0, l, f.off); f.off += w; total += w; }
    }
    wr32(nwritten, total); return 0;
  },
  fd_read(fd, iovs, n, nread) {
    refresh(); let total = 0;
    for (let i = 0; i < n; i++) {
      const p = rd32(iovs + i * 8), l = rd32(iovs + i * 8 + 4);
      const f = files.get(fd); if (!f) return 8;
      const tmp = Buffer.allocUnsafe(l); const r = readSync(f.rfd, tmp, 0, l, f.off); f.off += r;
      refresh(); u8.set(tmp.subarray(0, r), p); total += r; if (r < l) break;
    }
    wr32(nread, total); return 0;
  },
  fd_close(fd) { const f = files.get(fd); if (f) { try { closeSync(f.rfd); } catch {} files.delete(fd); } return 0; },
  fd_seek(fd, offset, whence, newOff) {
    const f = files.get(fd); if (!f) return 8; const o = Number(offset);
    if (whence === 0) f.off = o; else if (whence === 1) f.off += o; else f.off = fstatSync(f.rfd).size + o;
    wr64(newOff, f.off); return 0;
  },
  path_open(dirfd, dflags, pathP, pathL, oflags, rightsBase, rightsInh, fdflags, openedFd) {
    const p = str(pathP, pathL);
    let full; try { full = relResolve(p); } catch { return 44; }   // ENOENT
    let flag = 'r';
    if (oflags & 8) flag = 'w';                    // O_TRUNC
    else if (oflags & 1) flag = (fdflags & 1) ? 'a' : 'w';   // O_CREAT (+ APPEND)
    let rfd; try { rfd = openSync(full, flag); } catch { return 44; }
    const fd = nextFd++; files.set(fd, { rfd, off: 0 }); wr32(openedFd, fd); return 0;
  },
  args_sizes_get(argcP, bufP) {
    let bytes = 0; for (const a of ARGS) bytes += enc.encode(a).length + 1;
    wr32(argcP, ARGS.length); wr32(bufP, bytes); return 0;
  },
  args_get(argvP, bufP) {
    refresh(); let bp = bufP;
    for (let i = 0; i < ARGS.length; i++) { wr32(argvP + i * 4, bp); const b = enc.encode(ARGS[i]); u8.set(b, bp); bp += b.length; u8[bp++] = 0; }
    return 0;
  },
  // --- filesystem mutators (relative to the preopen) ---
  path_unlink_file(fd, pathP, pathL) {
    try { unlinkSync(relResolve(str(pathP, pathL))); return 0; } catch { return 44; }
  },
  path_create_directory(fd, pathP, pathL) {
    try { mkdirSync(relResolve(str(pathP, pathL))); return 0; } catch { return 44; }
  },
  path_remove_directory(fd, pathP, pathL) {
    try { rmdirSync(relResolve(str(pathP, pathL))); return 0; } catch { return 44; }
  },
  path_rename(oldFd, oldP, oldL, newFd, newP, newL) {
    try { renameSync(relResolve(str(oldP, oldL)), relResolve(str(newP, newL))); return 0; } catch { return 44; }
  },
  path_filestat_get(fd, flags, pathP, pathL, bufP) {
    let st; try { st = statSync(relResolve(str(pathP, pathL))); } catch { return 44; }
    refresh();
    // WASI filestat: dev(0) ino(8) filetype(16) nlink(24) size(32) atim(40) mtim(48) ctim(56)
    for (let i = 0; i < 64; i++) u8[bufP + i] = 0;
    wr64(bufP + 32, BigInt(Math.floor(st.size)));
    wr64(bufP + 40, BigInt(Math.round(st.atimeMs * 1e6)));
    wr64(bufP + 48, BigInt(Math.round(st.mtimeMs * 1e6)));
    wr64(bufP + 56, BigInt(Math.round(st.ctimeMs * 1e6)));
    return 0;
  },
  // --- clocks --- 0 = realtime, 1 = monotonic; result is nanoseconds
  clock_time_get(clockId, precision, timeP) {
    const ns = clockId === 0 ? BigInt(Date.now()) * 1000000n : hrtime.bigint();
    wr64(timeP, ns); return 0;
  },
  // --- entropy ---
  random_get(bufP, bufL) {
    refresh(); randomFillSync(u8, bufP, bufL); return 0;
  },
  // --- environment ---
  environ_sizes_get(countP, bufP) {
    const e = Object.entries(process.env);
    let bytes = 0; for (const [k, v] of e) bytes += enc.encode(`${k}=${v}`).length + 1;
    wr32(countP, e.length); wr32(bufP, bytes); return 0;
  },
  environ_get(environP, bufP) {
    refresh(); const e = Object.entries(process.env); let bp = bufP;
    for (let i = 0; i < e.length; i++) { wr32(environP + i * 4, bp); const b = enc.encode(`${e[i][0]}=${e[i][1]}`); u8.set(b, bp); bp += b.length; u8[bp++] = 0; }
    return 0;
  },
};

const { instance } = await WebAssembly.instantiate(readFileSync(modPath), { wasi_snapshot_preview1: wasi });
mem = instance.exports.memory;
try { instance.exports._start(); }
catch (e) { if (!(e instanceof ExitErr)) { console.error('[trap]', e.constructor.name, e.message); exitCode = 70; } }
process.exit(exitCode);
