#!/bin/bash
# Bootstrap test for register cache feature
# Builds gen1 through gen4 and verifies deterministic codegen
set -e

echo "=== Step 1: Build gen1 from current source ==="
./caustic src/main.cst 2>&1 | tail -3
./caustic-as src/main.cst.s
./caustic-ld src/main.cst.s.o -o /tmp/gen1
echo "gen1 size: $(stat -c%s /tmp/gen1)"

echo ""
echo "=== Step 2: gen1 -O1 -> gen2 ==="
/tmp/gen1 -O1 src/main.cst 2>&1 | tail -3
./caustic-as src/main.cst.s
./caustic-ld src/main.cst.s.o -o /tmp/gen2
echo "gen2 size: $(stat -c%s /tmp/gen2)"

echo ""
echo "=== Step 3: gen2 -O1 -> gen3 ==="
/tmp/gen2 -O1 src/main.cst 2>&1 | tail -3
./caustic-as src/main.cst.s
./caustic-ld src/main.cst.s.o -o /tmp/gen3
echo "gen3 size: $(stat -c%s /tmp/gen3)"

echo ""
echo "=== Step 4: gen3 -O1 -> gen4 ==="
/tmp/gen3 -O1 src/main.cst 2>&1 | tail -3
./caustic-as src/main.cst.s
./caustic-ld src/main.cst.s.o -o /tmp/gen4
echo "gen4 size: $(stat -c%s /tmp/gen4)"

echo ""
echo "=== Results ==="
S2=$(stat -c%s /tmp/gen2)
S3=$(stat -c%s /tmp/gen3)
S4=$(stat -c%s /tmp/gen4)
echo "gen2=$S2  gen3=$S3  gen4=$S4"
if [ "$S2" = "$S3" ] && [ "$S3" = "$S4" ]; then
    echo "PASS: gen2, gen3, gen4 are all the same size (deterministic codegen)"
else
    echo "FAIL: sizes differ — non-deterministic codegen"
    exit 1
fi

echo ""
echo "=== Step 5: Test with test_cache.cst ==="
/tmp/gen1 examples/test_cache.cst 2>&1 | tail -3
./caustic-as examples/test_cache.cst.s
./caustic-ld examples/test_cache.cst.s.o -o /tmp/test_cache
/tmp/test_cache; echo "Exit: $?"

echo ""
echo "=== Step 6: Test with -O1 ==="
/tmp/gen1 -O1 examples/test_cache.cst 2>&1 | tail -3
./caustic-as examples/test_cache.cst.s
./caustic-ld examples/test_cache.cst.s.o -o /tmp/test_cache_o1
/tmp/test_cache_o1; echo "Exit: $?"
