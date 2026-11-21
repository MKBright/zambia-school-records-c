#!/bin/bash
# Drives the menu the way a school clerk would, using a piped script.
# Run from the project root via `make test`.

set -euo pipefail

cd "$(dirname "$0")/.."

if [[ ! -x build/schoolreg ]]; then
    echo "Build the program first (make)." >&2
    exit 1
fi

TMP="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP"
}
trap cleanup EXIT

cp data/sample_learners.csv "$TMP/learners.csv"
REPORT="$TMP/grade10A-term1.txt"
OUT="$TMP/out.txt"

fail() {
    echo "E2E FAIL: $1" >&2
    if [[ -f "$OUT" ]]; then
        echo "----- program output -----" >&2
        cat "$OUT" >&2
    fi
    exit 1
}

# No file yet: the program must start empty instead of crashing.
printf '0\n' | ./build/schoolreg "$TMP/missing.csv" > "$OUT"
grep -q "No saved register" "$OUT" || fail "missing file should start empty"
grep -q "Goodbye" "$OUT" || fail "exit should say goodbye"

# Too many arguments is a usage error.
if ./build/schoolreg one two > "$OUT" 2>"$TMP/err.txt"; then
    fail "two arguments should be rejected"
fi
grep -q "Usage:" "$TMP/err.txt" || fail "usage text missing"

cat > "$TMP/session1.txt" <<EOF
5
4
Zulu
8
10
A
1
y
$REPORT
1
2410-100
Test Learner
10
A
F
0971111111
6
2410-100
1
10
20
30
40
50
60
70
80
7
2410-100
9
0
EOF

./build/schoolreg "$TMP/learners.csv" < "$TMP/session1.txt" > "$OUT"

grep -q "Loaded 11 learners" "$OUT" || fail "sample did not load"
grep -q "Thandiwe Zulu" "$OUT" || fail "list should show Thandiwe"
grep -q "1 match." "$OUT" || fail "search for Zulu"
grep -q "Class average: 67.5" "$OUT" || fail "grade 10A average"
grep -q "not an official Examinations Council of Zambia result" "$OUT" || fail "ECZ note"
grep -q "Added Test Learner" "$OUT" || fail "add learner"
grep -q "Total: 360" "$OUT" || fail "recorded total"
grep -q "Average: 45.0" "$OUT" || fail "recorded average"
grep -q "Approx. grade: 7 Satisfactory" "$OUT" || fail "overall band"
grep -q "Saved 12 learners" "$OUT" || fail "save after adding"
grep -q "2410-100" "$TMP/learners.csv" || fail "csv should keep the new learner"

[[ -f "$REPORT" ]] || fail "text report was not written"
grep -E '^1 +2410-004' "$REPORT" >/dev/null || fail "Thandiwe should be position 1"
grep -E '^3 +2410-006' "$REPORT" >/dev/null || fail "Chanda should share position 3"
grep -E '^3 +2410-002' "$REPORT" >/dev/null || fail "Mwansa should share position 3"
grep -E '^5 +2410-003' "$REPORT" >/dev/null || fail "Kondwani should be position 5"
grep -E '^6 +2410-005' "$REPORT" >/dev/null || fail "Joseph should be position 6"
grep -q "A blank is not a zero" "$REPORT" || fail "blank-mark note on the printout"

cat > "$TMP/session2.txt" <<'EOF'
4
2410-100
3
2410-100
YES
9
0
EOF

./build/schoolreg "$TMP/learners.csv" < "$TMP/session2.txt" > "$OUT"
grep -q "Test Learner" "$OUT" || fail "saved learner should load again"
grep -q "Deleted Test Learner" "$OUT" || fail "delete"
grep -q "Saved 11 learners" "$OUT" || fail "save after delete"
if grep -q "2410-100" "$TMP/learners.csv"; then
    fail "deleted learner should leave the csv"
fi

cat > "$TMP/session3.txt" <<'EOF'
4
2410-100
0
EOF

./build/schoolreg "$TMP/learners.csv" < "$TMP/session3.txt" > "$OUT"
grep -q 'No learner matched "2410-100"' "$OUT" || fail "deleted learner should not be found"

echo "End-to-end menu checks passed."
