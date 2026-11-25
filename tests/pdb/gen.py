# program: two modules, live, dead and inlinable functions; each statement of
# interest carries a unique constant, so an instruction names its source line
import sys
def mod(name, base, n, other=None):
    L=[]; consts={}
    if other: L.append('use "%s.cst" as o;'%other)
    L.append('let is i64 as g_%s with mut = 1;'%name)
    for i in range(n):
        c1=0x5A000000+base*0x10000+i*16+1; c2=c1+1; c3=c1+2
        ni='' if i%4==3 else ' with noinline'
        L.append('fn f_%s_%d(a as i64, b as i64) as i64%s {'%(name,i,ni))
        L.append('    let is i64 as t with mut = a;')
        L.append('    g_%s = g_%s + (b ^ %d);'%(name,name,c1)); consts[c1]=len(L)
        if i%3==0:
            L.append('    while (t > 10) {')
            L.append('        t = t - (g_%s ^ %d);'%(name,c2)); consts[c2]=len(L)
            L.append('    }')
        L.append('    g_%s = t ^ (g_%s ^ %d);'%(name,name,c3)); consts[c3]=len(L)
        L.append('    return t + g_%s;'%name)
        L.append('}')
    return L,consts
n=24
a,ca=mod('a',1,n); b,cb=mod('b',2,n,'a')
b.append('fn main(argc as i64) as i32 {'); b.append('    let is i64 as s with mut = 0;')
for i in range(n):
    if i%5==2: continue           # never called: dropped by DCE
    b.append('    s = s + f_b_%d(argc, s);'%i)
    b.append('    s = s + o.f_a_%d(argc, s);'%i)
b.append('    return cast(i32, s & 1);'); b.append('}')
open('a.cst','w').write('\n'.join(a)+'\n'); open('b.cst','w').write('\n'.join(b)+'\n')
import json; json.dump({'a':ca,'b':cb},open('consts.json','w'))
