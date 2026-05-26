#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════════════════
#  Test Runner for Dead Instruction Eliminator Pass
# ═══════════════════════════════════════════════════════════════════════════
#
#  Usage:
#    ./tests/run_tests.sh                       # uses opt + plugin
#    ./tests/run_tests.sh --tool ./build/die-tool  # uses standalone tool
#
# ═══════════════════════════════════════════════════════════════════════════

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
TESTS_DIR="$SCRIPT_DIR"
PASS_LIB="$BUILD_DIR/libDeadInstructionEliminator.so"
DIE_TOOL=""
USE_OPT=true

# Find opt command
OPT_CMD="opt"
if ! command -v opt &>/dev/null; then
  for val in opt-17 opt-16 opt-15 opt-14; do
    if command -v "$val" &>/dev/null; then
      OPT_CMD="$val"
      break
    fi
  done
fi

# ── Parse arguments ───────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case "$1" in
    --tool)
      DIE_TOOL="$2"
      USE_OPT=false
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# ── Colours ───────────────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Colour

PASS=0
FAIL=0
TOTAL=0

# ── Test function ─────────────────────────────────────────────────────────
run_test() {
  local test_file="$1"
  local test_name
  test_name="$(basename "$test_file" .ll)"
  local output_file="$TESTS_DIR/${test_name}_opt.ll"
  TOTAL=$((TOTAL + 1))

  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${YELLOW}  TEST: ${test_name}${NC}"
  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

  if $USE_OPT; then
    if ! "$OPT_CMD" -load-pass-plugin "$PASS_LIB" \
         -passes="dead-instruction-eliminator" \
         -S "$test_file" -o "$output_file" 2>&1; then
      echo -e "  ${RED}FAIL${NC} – opt returned non-zero exit code"
      FAIL=$((FAIL + 1))
      return
    fi
  else
    if ! "$DIE_TOOL" "$test_file" -o "$output_file" -v 2>&1; then
      echo -e "  ${RED}FAIL${NC} – die-tool returned non-zero exit code"
      FAIL=$((FAIL + 1))
      return
    fi
  fi

  echo ""
  echo -e "  ${GREEN}✓ PASS${NC} – Optimized IR written to $output_file"
  echo ""

  # Show a diff summary
  if command -v diff &>/dev/null; then
    local diff_count
    diff_count=$(diff "$test_file" "$output_file" | grep -c '^[<>]' || true)
    if [ "$diff_count" -gt 0 ]; then
      echo -e "  → ${diff_count} line(s) changed"
    else
      echo -e "  → No changes (all instructions were live)"
    fi
  fi

  # Verify the output IR is valid
  if command -v "$OPT_CMD" &>/dev/null; then
    if "$OPT_CMD" -passes=verify -S "$output_file" -o /dev/null 2>&1; then
      echo -e "  → ${GREEN}IR verification passed${NC}"
    else
      echo -e "  → ${RED}IR verification FAILED${NC}"
      FAIL=$((FAIL + 1))
      return
    fi
  fi

  PASS=$((PASS + 1))
  echo ""
}

# ── Run all tests ─────────────────────────────────────────────────────────

echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║   Dead Instruction Eliminator – Test Suite                  ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

for test_file in "$TESTS_DIR"/test[1-5].ll; do
  run_test "$test_file"
done

# ── Summary ───────────────────────────────────────────────────────────────
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                     TEST SUMMARY                            ║"
echo "╠══════════════════════════════════════════════════════════════╣"
echo -e "║  Total : ${TOTAL}                                               ║"
echo -e "║  Passed: ${GREEN}${PASS}${NC}                                               ║"
echo -e "║  Failed: ${RED}${FAIL}${NC}                                               ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

if [ "$FAIL" -gt 0 ]; then
  exit 1
fi
