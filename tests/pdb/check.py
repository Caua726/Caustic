# usage: check.py <exe> <pdb> <consts.json>
# Every instruction holding one of gen.py's constants must be attributed, by
# llvm-pdbutil's line table and by llvm-symbolizer, to the line that wrote it.
import subprocess, re, json, sys
exe, pdb, cj = sys.argv[1:4]
C = json.load(open(cj))
def out(*a, inp=None):
    r = subprocess.run(a, capture_output=True, text=True, errors='replace', input=inp)
    if r.returncode: print('FAIL', a[0], 'exit', r.returncode, r.stderr[:200]); sys.exit(1)
    return r.stdout
va = [int(x, 16) for x in re.findall(r'VirtualAddress: (0x[0-9A-F]+)', out('llvm-readobj', '--sections', exe))]
base = int(re.search(r'ImageBase: (0x[0-9A-F]+)', out('llvm-readobj', '--file-headers', exe)).group(1), 16)
ins = []
for m in re.finditer(r'^\s*([0-9a-f]+):\s+(.*)$', out('llvm-objdump', '-d', '--no-show-raw-insn', '--print-imm-hex', '-M', 'intel', exe), re.M):
    for k in re.findall(r'0x(5a[0-9a-f]{6})\b', m.group(2)):
        for f in C:
            if str(int(k, 16)) in C[f]: ins.append((int(m.group(1), 16), f, C[f][str(int(k, 16))]))
tab = []; cur = None
for ln in out('llvm-pdbutil', 'dump', '-l', pdb).splitlines():
    m = re.match(r'^(\S.*\.cst) \(', ln)
    if m: cur = m.group(1); continue
    m = re.match(r'^\s+(\d{4}):([0-9A-F]{8})-([0-9A-F]{8}), line', ln)
    if m: tab.append((int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16), cur, [])); continue
    if tab:
        for a, b in re.findall(r'(\d+) ([0-9A-F]{8})\b', ln): tab[-1][4].append((int(b, 16), int(a)))
sym = out('llvm-symbolizer', '--obj=' + exe, '--no-inlines', inp=''.join('0x%x\n' % a for a, _, _ in ins)).strip().split('\n\n')
bad = 0
for (a, f, want), s in zip(ins, sym):
    off = a - base - va[0]; got = None
    for seg, lo, hi, fl, es in tab:
        c = [e for e in es if e[0] <= off]
        if seg == 1 and lo <= off < hi and c: got = (fl, max(c, key=lambda e: e[0])[1])
    if not got or got[1] != want or not got[0].endswith(f + '.cst'):
        bad += 1; print('FAIL %x: want %s.cst:%d, line table %s' % (a, f, want, got))
    fl = s.split('\n')[-1]
    if not re.search(r'(^|[/\\])%s\.cst:%d(:|$)' % (f, want), fl):
        bad += 1; print('FAIL %x: want %s.cst:%d, symbolizer %s' % (a, f, want, fl))
    if bad > 6: break
if len(ins) < 20: bad += 1; print('FAIL only %d constants found' % len(ins))
syms = out('llvm-pdbutil', 'dump', '--symbols', pdb)
print('%d constants in %d line blocks, %d procs, %d bad' % (len(ins), len(tab), syms.count('S_GPROC32'), bad))
sys.exit(1 if bad else 0)
