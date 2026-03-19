# Audit: i32 SSA promotion pipeline

## Objetivo

Habilitar `addr_fusion` antes do `mem2reg` para que variáveis i32 sejam promovidas a SSA vregs. Isso elimina ~100-200ms do benchmark removendo movsxd + stack loads/stores redundantes em loops.

## O problema

Quando `addr_fusion` roda antes do `mem2reg`, os i32 slots são promovidos corretamente. Mas os passes seguintes crasham ou geram código errado porque não lidam bem com os phi vregs resultantes de variáveis i32.

O `mem2reg` de variáveis i32 produz IR diferente do i64:
- O phi vreg carrega um valor i32 (4 bytes) em um registrador i64 (8 bytes)
- O `mem2reg` insere `IR_CAST` (sign-extension) quando o tipo do slot é i32
- Os vregs resultantes têm `cast_to` apontando pra tipo i32
- Os passes precisam preservar essa semântica de truncação/extensão

## Passes que precisam de auditoria

### 1. pass_const_copy_prop (`opt_passes.cst:52-381`)

**Risco:** ALTO — faz constant folding e copy propagation

**Problemas potenciais:**
- Copy propagation de um vreg i32 (sign-extended) pode criar chains onde o truncamento é perdido
- Constant folding pode produzir um IR_IMM com valor i64 quando deveria ser i32-truncado
- O guard `ccp_defs` (multi-def) já protege phi vregs — provavelmente ok
- MAS: se o IR_CAST inserido pelo mem2reg for tratado como MOV pelo copy prop, o truncamento se perde

**O que verificar:**
- O IR_CAST é ignorado pelo copy propagation? (deveria ser — não é IR_MOV)
- Valores constantes folded de operações i32 são truncados a 32 bits?

**Teste:** compilar uma função com loop i32 que faz aritmética:
```cst
fn test() as i32 {
    let is i32 as sum with mut = 0;
    let is i32 as i with mut = 0;
    while (i < 100) { sum = sum + i; i = i + 1; }
    return sum;
}
```
Compilar com -O1, verificar que o resultado é correto (4950).

### 2. pass_store_load_fwd (`opt_passes.cst:487-580`)

**Risco:** BAIXO — já filtra por `ld_sz == 8` e `st_sz == 8`

**Problema:** Com i32 promovido a SSA, não há mais STORE/LOAD i32 no stack — o forwarding não se aplica. Slots i32 foram convertidos a vregs pelo mem2reg. Então esse pass provavelmente não é afetado.

**O que verificar:**
- Se algum STORE/LOAD i32 sobrevive depois do mem2reg (edge cases)
- Se o forwarding tenta forward um vreg i64 pra um slot que era i32

### 3. pass_fold_immediates (`opt_passes.cst:590-630`)

**Risco:** MÉDIO — folda constantes em operandos

**Problemas potenciais:**
- Se folda uma constante num IR_ADD que opera em i32, o resultado pode precisar de truncação
- O fold verifica `is_const[src2]` — se src2 é um vreg i32 de um phi, pode ser marcado const incorretamente (mas o ccp_defs guard já protege isso)

**O que verificar:**
- Valores foldados para IR_SHL com shift count i32 (0-63 range ok)
- IR_ADD com imediato i32 — o codegen gera `add DWORD PTR` ou `add QWORD`?

### 4. pass_licm (`opt_passes.cst:701-780`)

**Risco:** MÉDIO — move instruções pra fora de loops

**Problemas potenciais:**
- Se um IR_CAST (sign-extension) é hoisted fora do loop, o valor sign-extended pode ser usado incorretamente dentro do loop quando o vreg fonte muda a cada iteração
- O LICM verifica se os operandos são definidos fora do loop — IR_CAST com src definido dentro NÃO será hoisted (correto)

**O que verificar:**
- IR_IMM de constantes i32 dentro de loops — pode ser hoisted (correto, constantes não mudam)
- IR_CAST com src que é loop-invariant — pode ser hoisted (correto, extensão de valor invariant)

### 5. pass_strength_reduction (`opt_passes.cst:782-920`)

**Risco:** BAIXO — só atua em SHL dentro de loops

**O que verificar:**
- Se o SHL de um índice i32 funciona corretamente quando o índice é um vreg promovido

### 6. pass_dce (`opt_passes.cst:630-700`)

**Risco:** ALTO — a sessão anterior encontrou um bug aqui

**Bug encontrado:** O DCE marcava IR_MOV do phi back-edge como dead quando o dest vreg tinha múltiplas definições. Fix aplicado: multi-def guard no DCE.

**O que verificar:**
- O fix do multi-def guard está funcionando?
- IR_CAST inserido pelo mem2reg não é removido pelo DCE (tem side-effect? não — é puro)
- Se o IR_CAST é removido porque o dest vreg é "unused", mas na verdade o dest É usado pelo phi

### 7. pass_final_resolve (`opt_passes.cst:675-696`)

**Risco:** BAIXO — só resolve copy chains

**O que verificar:**
- Se resolve_alias ignora IR_CAST (deveria — IR_CAST não é IR_MOV, não cria copy_of entry)

### 8. pass_inline (`opt_passes.cst:800-935`)

**Risco:** BAIXO pra este bug — inline só atua em funções pequenas sem stack

### 9. ssa.mem2reg (`ssa.cst`)

**Risco:** ALTO — é o pass que faz a promoção

**O que verificar:**
- O IR_CAST inserido pra i32 slots tem `cast_to` e `cast_from` corretos?
- O phi destruction (phi → MOV) preserva o tipo i32?
- Se o valor que entra no phi pela back-edge é sign-extended corretamente

**Código relevante (ssa.cst:339-349):**
```cst
if (cast(i64, inst.cast_to) != 0) {
    let is *i32 as szp = cast(*i32, cast(i64, inst.cast_to) + 28);
    if (*szp != 8) {
        // Insert CAST for non-8-byte types
        inst.op = d.IR_CAST;
        inst.cast_from = inst.cast_to;
    }
}
```
Este código converte o LOAD em IR_CAST quando o tipo não é 8 bytes. O CAST trunca o valor pra i32 e sign-extends de volta pra i64. Isso deveria funcionar se o codegen emite `movsxd` pra IR_CAST com tipo i32.

## Método de debugging

### Step 1: Habilitar addr_fusion antes de mem2reg
```cst
// Em opt.cst:
passes.pass_addr_fusion(func, vc_alloc);  // ANTES do mem2reg
ssa.mem2reg(func);
```

### Step 2: Testar incrementalmente
Habilitar um pass por vez depois do mem2reg e testar com cada example:

```bash
rm -rf .caustic

# Test 1: só mem2reg (nenhum pass depois)
# → deveria funcionar (mem2reg standalone é testado)

# Test 2: + const_copy_prop
# → se crashar, o bug tá no const_copy_prop com phi vregs i32

# Test 3: + store_load_fwd
# Test 4: + fold_immediates
# Test 5: + licm
# Test 6: + dce
# Test 7: + final_resolve
# Test 8: + inline

# Pra cada, testar:
for ex in hello fibonacci structs enums linked_list bench test_arr; do
    /tmp/gen1 -O1 examples/$ex.cst 2>/dev/null && \
    ./caustic-as examples/$ex.cst.s 2>/dev/null && \
    ./caustic-ld examples/$ex.cst.s.o -o /tmp/t 2>/dev/null && \
    timeout 30 /tmp/t > /dev/null 2>&1
    echo "$ex: $?"
done
```

### Step 3: Quando achar o pass que quebra
Olhar o assembly da função que crasha. Comparar com a versão sem o pass. Identificar qual instrução foi incorretamente otimizada.

### Step 4: Corrigir o pass
O fix geralmente é um dos seguintes patterns:
- **Ignorar vregs com múltiplas definições** (ccp_defs guard — já implementado em const_copy_prop)
- **Não tratar IR_CAST como IR_MOV** (copy propagation não deve seguir CAST)
- **Preservar cast_to/cast_from ao clonar instruções** (inline, LICM)

### Step 5: Bootstrap gen4-O1
```bash
for g in 2 3 4; do
    /tmp/gen$((g-1)) -O1 src/main.cst && \
    ./caustic-as src/main.cst.s && \
    ./caustic-ld src/main.cst.s.o -o /tmp/gen$g
    echo "gen$g: $(stat -c%s /tmp/gen$g)"
done
# gen2=gen3=gen4
```

## Arquivos

| Arquivo | O que auditar |
|---------|-------------|
| `src/ir/opt.cst` | Mover addr_fusion antes de mem2reg |
| `src/ir/opt_passes.cst` | Cada pass: como lida com IR_CAST, phi vregs i32, multi-def |
| `src/ir/ssa.cst` | mem2reg: IR_CAST insertion, phi destruction pra i32 |
| `src/codegen/emit.cst` | gen_inst_memory/IR_CAST: emite movsxd correto pra i32 |

## Impacto esperado

Se a auditoria resultar em todos os passes funcionando com i32 SSA:
- Sieve init loop: 14 insns → ~5 insns (-64%)
- Matmul inner loop: menos movsxd (18 → ~4)
- Collatz: menos stack loads
- Benchmark total: ~972ms → ~750-800ms (-15 a -20%)

## Gotchas Caustic

- `with mut` pra variáveis mutáveis
- `rm -rf .caustic` antes de rebuildar
- `fn` e `gen` são keywords
- Commits: lowercase, sem Co-Authored-By
- `cast(i64, x)` pra i32→i64
- `0 - 1` em vez de `-1`
- Acessar tipo: `cast(*i32, cast(i64, inst.cast_to) + 28)` → campo `size` do Type struct (offset 28)
