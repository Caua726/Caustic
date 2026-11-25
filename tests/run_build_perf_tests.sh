#!/bin/sh
# What a build does, as counts: a limit on time fails on a busy machine, a limit on
# how often a list is walked does not. Each case is a defect that made a build of
# ~200k lines take 23 s where 3 were enough.
# Usage: sh tests/run_build_perf_tests.sh [compiler]
set -eu
cc=${1:-${CAUSTIC_TEST_COMPILER:-./caustic}}
case "$cc" in /*) ;; *) cc="$PWD/$cc" ;; esac
export CAUSTIC_LIB="${CAUSTIC_LIB:-$PWD}"
work=$(mktemp -d "${TMPDIR:-/tmp}/caustic-perf.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cd "$work"
# The default object cache would otherwise be the user's ~/.cache.
export XDG_CACHE_HOME="$work/xdg"     # not created: the compiler makes it
fail=0
check() { # what, ok(0/1)
    if [ "$2" = 1 ]; then echo "ok   $1"; else echo "FAIL $1"; fail=1; fi
}
stat_of() { sed -n "s/^stats: $2 //p" "$1"; }

# Three modules; `lots` holds S strings and G string globals.
S=300; G=300
{
    i=0
    while [ $i -lt $G ]; do echo "let is *u8 as g$i with imut = \"global $i\";"; i=$((i + 1)); done
    echo 'fn sum() as i64 {'
    echo '    let is i64 as n with mut = 0;'
    i=0
    while [ $i -lt $S ]; do echo "    n = n + cast(i64, \"literal $i\"[0]);"; i=$((i + 1)); done
    i=0
    while [ $i -lt $G ]; do echo "    n = n + cast(i64, g$i[0]);"; i=$((i + 1)); done
    echo '    return n;'
    echo '}'
} > lots.cst
echo 'fn two() as i64 { return 2; }' > small.cst
cat > main.cst <<'EOT'
use "lots.cst" as lots;
use "small.cst" as small;
fn main() as i32 { return cast(i32, (lots.sum() + small.two()) & 1); }
EOT

for level in 0 1 2; do
    "$cc" -q -O$level --stats -j 2 main.cst -o p$level 2> s$level
    modules=$(stat_of s$level modules)
    visits=$(stat_of s$level rodata_global_visits)
    # Linear: one pass over the globals per module, and each global's own string.
    limit=$(( (G + S + 16) * 2 * modules ))
    [ "$visits" -le "$limit" ] && r=1 || r=0
    check "-O$level emit_rodata visits $visits <= $limit" $r
done
calls=$(stat_of s2 outside_callers_calls)
[ "$calls" = 1 ] && r=1 || r=0
check "-O2 looks for outside callers once, not once per module ($calls)" $r

live=$(stat_of s2 tokens_live_bytes)
[ "$live" = 0 ] && r=1 || r=0
check "imported modules' tokens are gone before emission ($live bytes)" $r

# --emit-deps is a question about the module graph: it answers and stops.
rm -f main.cst.s deps.txt
"$cc" -q --stats --emit-deps deps.txt main.cst 2> sd
[ -s deps.txt ] && r=1 || r=0
check "--emit-deps writes the module list" $r
[ ! -e main.cst.s ] && r=1 || r=0
check "--emit-deps leaves no assembly beside the source" $r
emitted=$(stat_of sd funcs_emitted)
[ "$emitted" = 0 ] && r=1 || r=0
check "--emit-deps generates no code ($emitted functions)" $r

# Large working arrays are reused between functions, not mapped for each one.
{
    f=0
    while [ $f -lt 20 ]; do
        echo "fn big$f(a as i64) as i64 {"
        echo '    let is i64 as x with mut = a;'
        i=0
        while [ $i -lt 1500 ]; do echo "    if (x > $i) { x = x + $i; } else { x = x - $f; }"; i=$((i + 1)); done
        echo '    return x;'
        echo '}'
        f=$((f + 1))
    done
    echo 'fn main() as i32 {'
    echo '    let is i64 as t with mut = 0;'
    f=0
    while [ $f -lt 20 ]; do echo "    t = t + big$f(t);"; f=$((f + 1)); done
    echo '    return cast(i32, t & 1);'
    echo '}'
} > bigfn.cst
"$cc" -q -O2 --stats -j 1 bigfn.cst -o bigfn 2> sb
maps=$(stat_of sb large_maps)
[ "$maps" -le 700 ] && r=1 || r=0
check "-O2 maps $maps large blocks for 20 large functions (limit 700; it was 2167)" $r

# Only what main reaches gets code. Everything below is kept alive by something other
# than a plain call, which is what a reachability walk gets wrong by forgetting it.
mkdir dce && cd dce
cat > util.cst <<'EOT'
fn dead_code() as i64 { let is *u8 as s = "never_called_marker"; return cast(i64, s[0]); }
fn from_asm() as i64 { return 7; }
fn through_global() as i64 { return 11; }
fn in_section() as i64 with section(".perf_keep") { return 1; }
fn through_pointer() as i64 { return 3; }
fn pointer() as *u8 { return fn_ptr(through_pointer); }
fn wrap() as i64 with naked { asm("jmp _util_cst_from_asm\n"); }
let is *u8 as ptr_global with mut = fn_ptr(through_global);
let is i64 as kept with section(".perf_data") = 99;
let is *u8 as dead_global with mut = "dead_global_marker";
EOT
cat > main.cst <<'EOT'
use "util.cst" as u;
fn main() as i32 {
    let is i64 as a = u.wrap();                       // 7, only asm names it
    let is i64 as b = call(u.ptr_global);             // 11, a global's initializer
    let is i64 as c = 13;
    let is i64 as d = call(u.pointer());              // 3, fn_ptr in a body
    return cast(i32, a + b + c + d);                  // 34
}
EOT
for level in 0 1 2; do
    if "$cc" -q -O$level --stats main.cst -o d$level 2> ds$level; then r=1; else r=0; sed -n 1,6p ds$level; fi
    check "-O$level the program links" $r
    r=0; if [ -x d$level ]; then got=0; ./d$level || got=$?; [ "$got" = 34 ] && r=1; fi
    check "-O$level every indirect path still runs" $r
    r=0; if [ -x d$level ] && ! grep -q never_called_marker d$level; then r=1; fi
    check "-O$level an uncalled function's string is gone" $r
    r=0; if [ -x d$level ] && ! grep -q dead_global_marker d$level; then r=1; fi
    check "-O$level an unreferenced global's string is gone" $r
done
cd ..

# Bodies are parsed and lowered one at a time. That must not cost a diagnostic: an error
# in a function nothing calls, in an imported module, still stops the build, and what a
# template or an imut function needs of its body is still there when it is read later.
mkdir lazy && cd lazy
cat > bad_syntax.cst <<'EOT'
fn unused() as i64 { let is i64 as = 1; return 0; }
EOT
cat > bad_types.cst <<'EOT'
fn unused() as i64 { return undeclared_name; }
EOT
printf 'use "bad_syntax.cst" as b;\nfn main() as i32 { return 0; }\n' > m1.cst
printf 'use "bad_types.cst" as b;\nfn main() as i32 { return 0; }\n' > m2.cst
cat > lib.cst <<'EOT'
fn twice(x as i64) as i64 with imut { return x * 2; }
fn pick gen T (a as T, b as T) as T { if (a > b) { return a; } return b; }
struct Box gen T { v as T; }
impl Box gen T { fn get(self as *Box gen T) as T { return self.v; } }
EOT
cat > m3.cst <<'EOT'
use "lib.cst" as lib;
let is i64 as eight with imut = lib.twice(4);
fn main() as i32 {
    let is lib.Box gen i64 as b;
    b.v = lib.pick gen i64 (3, 34);
    return cast(i32, b.get() + eight);   // 42
}
EOT
for level in 0 1 2; do
    if "$cc" -q -O$level m1.cst -o o1 2> e1; then r=0; else r=1; fi
    grep -q "bad_syntax.cst" e1 || r=0
    check "-O$level a syntax error in an uncalled imported function is reported" $r
    if "$cc" -q -O$level m2.cst -o o2 2> e2; then r=0; else r=1; fi
    grep -q "bad_types.cst" e2 || r=0
    check "-O$level a type error in an uncalled imported function is reported" $r
    r=0
    if "$cc" -q -O$level m3.cst -o o3 2> e3; then got=0; ./o3 || got=$?; [ "$got" = 42 ] && r=1; else sed -n 1,4p e3; fi
    check "-O$level generics, impl methods and imut across modules" $r
done
cd ..

# The object cache: a warm build is byte for byte the clean build, and rebuilds only what
# it must. After each edit the program is built warm, then clean, and the two compared.
mkdir warm && cd warm
warm_sources() {
big_body() { i=0; while [ $i -lt 1300 ]; do echo "    x = x + $i;"; i=$((i + 1)); done; }
{
    echo 'fn big(a as i64) as i64 {'
    echo '    let is i64 as x with mut = a;'
    big_body
    echo '    return x;'
    echo '}'
    echo 'fn small(a as i64) as i64 { return a + 1; }'
    echo 'let is i64 as LIMIT with imut = 7;'
    echo 'struct P { x as i64; y as i64; }'
    echo 'fn pick gen T (a as T, b as T) as T { if (a > b) { return a; } return b; }'
} > leaf.cst
printf 'use "leaf.cst" as l;\nfn mid(a as i64) as i64 { let is *u8 as s = "mid"; return l.small(a) + cast(i64, s[0]) + l.pick gen i64 (a, 2); }\n' > mid.cst
printf 'fn other(a as i64) as i64 { let is *u8 as s = "other"; return a * cast(i64, s[1]); }\n' > other.cst
cat > top.cst <<'EOT'
use "leaf.cst" as l;
use "mid.cst" as m;
use "other.cst" as o;
fn main() as i32 {
    let is l.P as p;
    p.x = l.big(1);
    p.y = m.mid(p.x) + o.other(2) + l.LIMIT;
    return cast(i32, p.y & 127);
}
EOT
}
warm_check() { # what, level, edit command, max emitted
    eval "$3"
    "$cc" -q -O$2 --stats --obj-cache oc$2 -j 2 top.cst -o w$2 2> ws || { sed -n 1,4p ws; check "$1: builds" 0; return; }
    "$cc" -q -O$2 --no-obj-cache -j 2 top.cst -o c$2 2> /dev/null
    if cmp -s w$2 c$2; then r=1; else r=0; fi
    check "-O$2 $1: warm equals clean" $r
    emitted=$(stat_of ws objects_emitted)
    if [ "$emitted" -le "$4" ]; then r=1; else r=0; fi
    check "-O$2 $1: emitted $emitted of 4 (at most $4)" $r
}
for level in 0 1 2; do
    mkdir l$level && cd l$level
    warm_sources
    mkdir oc$level
    warm_check "cold" $level ":" 4
    warm_check "nothing changed" $level ":" 0
    warm_check "a comment" $level "echo '// note' >> other.cst" 0
    warm_check "a body the inliner refuses" $level "sed -i 's/x = x + 700;/x = x + 701;/' leaf.cst" 1
    warm_check "a small function, inlined by callers" $level "sed -i 's/return a + 1;/return a + 3;/' leaf.cst" 3
    warm_check "a string another module also holds" $level "sed -i 's/\"other\"/\"mid\"/' other.cst" 2
    warm_check "that string back" $level "sed -i '0,/\"mid\"/s//\"other\"/' other.cst" 2
    warm_check "an imut constant" $level "sed -i 's/LIMIT with imut = 7/LIMIT with imut = 9/' leaf.cst" 4
    warm_check "a struct's layout" $level "sed -i 's/struct P { x as i64; y as i64; }/struct P { y as i64; x as i64; }/' leaf.cst" 4
    warm_check "a generic's body" $level "sed -i 's/if (a > b) { return a; } return b;/if (a < b) { return a; } return b;/' leaf.cst" 4
    # A damaged object is built again, not linked.
    for o in oc$level/*.o; do printf 'junk' > "$o"; done
    warm_check "damaged objects" $level ":" 4
    warm_check "nothing changed again" $level ":" 0
    cd ..
done
cd ..

# The default object cache: one directory per configuration, reused warm, off
# under --no-obj-cache and the checking switches, and collected by age (30 days)
# then by size, at most once a day.
mkdir dflt && cd dflt
warm_sources
objs="$XDG_CACHE_HOME/caustic/objs"
"$cc" -q -O1 --stats -j 2 top.cst -o d1 2> s1
"$cc" -q -O1 --stats -j 2 top.cst -o d2 2> s2
before=$(ls -d "$objs"/*/)
d1dir=$(ls -td "$objs"/*/ | head -1)
check "default cache: cold emits 4" "$([ "$(stat_of s1 objects_emitted)" = 4 ] && echo 1 || echo 0)"
check "default cache: warm emits 0" "$([ "$(stat_of s2 objects_emitted)" = 0 ] && echo 1 || echo 0)"
"$cc" -q -O1 --no-obj-cache -j 2 top.cst -o d3
check "default cache: warm equals uncached" "$(cmp -s d2 d3 && echo 1 || echo 0)"
"$cc" -q -O1 --no-obj-cache --stats -j 2 top.cst -o d4 2> s4
check "--no-obj-cache reuses nothing" "$([ "$(stat_of s4 objects_emitted)" = 4 ] && echo 1 || echo 0)"
CAUSTIC_PRINT_CHECK=1 "$cc" -q -O1 --stats -j 2 top.cst -o d5 2> s5
check "a checking switch reuses nothing" "$([ "$(stat_of s5 objects_emitted)" = 4 ] && echo 1 || echo 0)"
"$cc" -q -O2 -j 2 top.cst -o e1
d2dir=$(ls -d "$objs"/*/ | grep -vxF "$before")
check "-O2 gets a directory of its own" "$([ "$(echo "$d2dir" | wc -w)" = 1 ] && echo 1 || echo 0)"
old=$(ls "$d2dir"*.o | head -2); keep=$(ls "$d2dir"*.o | sed -n 3p)
touch -d '40 days ago' $old; touch -d '29 days ago' "$keep"
touch -d '2 days ago' "$objs/.collected"
# Built at -O1, so nothing it emits replaces what was collected from -O2's directory.
"$cc" -q -O1 -j 2 top.cst -o d6
gone=1; for f in $old; do [ -e "$f" ] && gone=0; done
check "objects 40 days old are collected" $gone
check "an object 29 days old stays" "$([ -e "$keep" ] && echo 1 || echo 0)"
touch -d '40 days ago' "$keep"
"$cc" -q -O1 -j 2 top.cst -o d7
check "collected at most once a day" "$([ -e "$keep" ] && echo 1 || echo 0)"
touch -d '2 days ago' "$objs/.collected"
for f in "$objs"/*/*.o "$objs"/*/*.tk; do touch -d '10 minutes ago' "$f"; done
CAUSTIC_CACHE_MAX_MB=0 "$cc" -q -O1 -j 2 top.cst -o d8
check "over the size limit, the oldest go" "$([ -z "$(ls "$d2dir")" ] && echo 1 || echo 0)"
check "the build still links" "$(cmp -s d8 d3 && echo 1 || echo 0)"

# Imported modules' token lists (src/lexer/tokcache.cst) are kept in the same
# directory: a warm build reads them, and a damaged or stale one is a miss.
printf 'use "std/io.cst" as io;\nuse "tmod.cst" as m;\nfn main() as i32 { io.printf("%%d\\n", m.val()); return 0; }\n' > tk.cst
{ for i in $(seq 40); do echo "// a comment line so that the module is worth keeping"; done; echo 'fn val() as i64 { return 41; }'; } > tmod.cst
"$cc" -q -O1 tk.cst -o t1
ntk=$(ls "$objs"/*/*.tk 2>/dev/null | wc -l)
check "token lists are kept ($ntk)" "$([ "$ntk" -ge 5 ] && echo 1 || echo 0)"
"$cc" -q -O1 tk.cst -o t2; "$cc" -q -O1 --no-obj-cache tk.cst -o t3
check "warm token lists: same output" "$(cmp -s t2 t3 && cmp -s t1 t3 && echo 1 || echo 0)"
for f in "$objs"/*/*.tk; do head -c 700 "$f" > cut; cat cut > "$f"; done
"$cc" -q -O1 tk.cst -o t4
check "truncated token lists are lexed again" "$(cmp -s t4 t3 && echo 1 || echo 0)"
"$cc" -q -O1 tk.cst -o t5
for f in "$objs"/*/*.tk; do printf '\377\377\377\377\377\377\377\177' | dd of="$f" bs=1 seek=36 conv=notrunc 2>/dev/null; done
"$cc" -q -O1 tk.cst -o t6
check "a token outside its source is refused" "$(cmp -s t6 t3 && echo 1 || echo 0)"
sed -i 's/return 41/return 42/' tmod.cst
"$cc" -q -O1 tk.cst -o t7
check "an edited module is lexed again" "$([ "$(./t7)" = 42 ] && echo 1 || echo 0)"
printf 'let is i64 as bad = 1 \140 2;\n' >> tmod.cst
r1=0; "$cc" -q -O1 tk.cst -o t9 2> e1 || r1=1
r2=0; "$cc" -q -O1 tk.cst -o t9 2> e2 || r2=1
check "a lexer error is reported again when warm" "$([ $r1 = 1 ] && [ $r2 = 1 ] && [ -s e1 ] && cmp -s e1 e2 && echo 1 || echo 0)"
sed -i '$d' tmod.cst
rm -f "$objs"/*/*.tk
"$cc" -q -O1 --no-obj-cache tk.cst -o t8
check "--no-obj-cache keeps no token lists" "$([ -z "$(ls "$objs"/*/*.tk 2>/dev/null)" ] && echo 1 || echo 0)"
cd ..

[ "$fail" = 0 ] && echo 'build-perf: all counts within limits' || exit 1
