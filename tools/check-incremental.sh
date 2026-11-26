#!/usr/bin/env bash
# Incremental builds give what a build from source gives: every edit that another
# module's object can depend on reaches it, interface files that are missing,
# damaged or of other flags are not used, and generic instances are shared.
# Usage: tools/check-incremental.sh [directory of the toolchain]
R=$(cd "$(dirname "$0")/.." && pwd)
N=$(cd "${1:-$R}" && pwd)
export LC_ALL=C PATH=$N:$PATH CAUSTIC_DIR=$N CAUSTIC_LIB=$R
unset CAUSTIC_NO_OBJ_CACHE
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; f=0
edits() ( OPT=$1; T=$W/e; rm -rf "$T"; mkdir "$T"; cd "$T" || exit 1; f=0
printf 'name "k"\nversion "0.0.1"\ntarget "k" {\n    src "k.cst"\n    out "k"\n    flags "%s"\n}\n' "$OPT" > Causticfile
printf 'struct P { a as i64; b as i64; }\nlet is i64 as K with imut = 3;\nfn get(p as *P) as i64 { return p.b; }\nfn small(x as i64) as i64 { return x + 1; }\nfn big(x as i64) as i64 with noinline { return x; }\nfn id gen T (x as T) as T { return x; }\n' > l.cst
printf 'use "l.cst" as l;\nfn via() as i64 { return l.small(1); }\n' > m.cst
printf 'use "m.cst" as m;\nuse "l.cst" as l;\nfn main() as i32 { let is l.P as p with mut; p.a = 100; p.b = 5; return cast(i32, l.get(&p) + l.K + m.via() + l.big(0) + l.id gen i64 (0)); }\n' > k.cst
f=0; t() { o=$(caustic-mk build k --incremental 2>&1 </dev/null | tail -1); ./k; r=$?; c=$(rm -rf ../c; mkdir ../c; cp *.cst ../c; cd ../c; caustic -q $OPT k.cst -o kk >/dev/null && ./kk; echo $?); [ $r = $c ] && m=ok || { m=BAD; f=1; }; [ $m = ok ] || echo "$m $OPT $1: $r/$c  $o"; }
t cold; t nochange
sed -i 's/return x;/return x + 2;/' l.cst; t noinline-body
sed -i 's/a as i64; b as i64/b as i64; a as i64/' l.cst; t layout
sed -i 's/imut = 3/imut = 30/' l.cst; t const
sed -i 's/x + 1;/x + 20;/' l.cst; t inlined-body
sed -i 's/(x as T) as T { return x + 2/(x as T) as T { return x + 3/' l.cst; t generic
sed -i 's/x + 20/x + 1/' l.cst; t back
sed -i 's/x + 1;/x + 20;/' l.cst; t forth
printf 'fn extra() as i64 { return 7; }\n' > n.cst; sed -i '1i use "n.cst" as n;' m.cst; sed -i 's/return l.small(1)/return l.small(1) + n.extra()/' m.cst; t new-use
sed -i 's/ + n.extra()//; /n.cst/d' m.cst; rm n.cst; t dropped-use
echo '// x' >> l.cst; t comment
printf 'garbage' > $(ls .caustic/csti/*/*_l_cst.csti | head -1); echo '// y' >> k.cst; t corrupt-csti
for x in .caustic/csti/*/*.csti; do python3 -c "
import sys;b=bytearray(open(sys.argv[1],'rb').read());b[len(b)//2]^=255;open(sys.argv[1],'wb').write(b)" $x; done; echo '// z' >> k.cst; t flipped-csti
rm -rf .caustic/csti; echo '// w' >> m.cst; t no-csti
sed -i 's/flags "[^"]*"/flags "-O1"/' Causticfile; OPT=-O1; sed -i 's/x + 20/x + 9/' l.cst; t other-flags
touch -d 2001-01-01 l.cst; sed -i 's/x + 9/x + 8/' l.cst; touch -d 2001-01-01 l.cst; t old-date
exit $f )
generics() ( O=$1; T=$W/g; rm -rf "$T"; mkdir "$T"; cd "$T" || exit 1
printf 'let is i64 as cnt with mut = 0;\nfn inn gen T (x as T) as T with noinline { cnt = cnt + 1; return x; }\nfn out gen T (x as T) as T with noinline { return inn gen T (x); }\nfn ptr gen T (x as T) as *u8 with noinline { return fn_ptr(inn gen i64); }\n' > g.cst
printf 'use "g.cst" as g;\nfn a() as i64 with noinline { return g.out gen i64 (3); }\nfn p() as *u8 { return g.ptr gen i64 (0); }\n' > a.cst
printf 'use "g.cst" as g;\nfn b() as i64 with noinline { return g.out gen i64 (4) + cast(i64, g.out gen i32 (1)); }\nfn p() as *u8 { return g.ptr gen i64 (0); }\n' > b.cst
printf 'use "g.cst" as g;\nuse "a.cst" as a;\nuse "b.cst" as b;\nfn main() as i32 { let is i64 as r with mut = a.a() + b.b(); if (cast(i64, a.p()) == cast(i64, b.p())) { r = r + 100; } return cast(i32, r + g.cnt * 10); }\n' > k.cst
printf 'name "k"\nversion "0.0.1"\ntarget "k" {\n    src "k.cst"\n    out "k"\n    flags "%s"\n}\n' "$O" > Causticfile
caustic-mk build k --incremental >l 2>&1 </dev/null || tail -3 l; ./k; r1=$?; echo "// z" >> a.cst; echo "fn u() as i64 { return 1; }" >> g.cst; caustic-mk build k --incremental >l 2>&1 </dev/null || tail -3 l; ./k; r2=$?
[ $r1 = 138 ] && [ $r2 = 138 ] || { echo "BAD $O generic instances: $r1 $r2, want 138"; exit 1; } )
for l in -O0 -O1 -O2; do edits $l || f=1; generics $l || f=1; done
[ $f = 0 ] && echo "incremental: 54 builds as from source"
exit $f
