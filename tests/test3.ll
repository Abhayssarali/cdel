; ═══════════════════════════════════════════════════════════════════════════
; TEST 3: Mixed Live/Dead with Side Effects
; ═══════════════════════════════════════════════════════════════════════════
;
; Description:
;   This test mixes side-effecting instructions (stores, calls) with
;   dead arithmetic.  The pass must PRESERVE side-effecting instructions
;   even when their results are unused, while eliminating purely dead
;   value computations.
;
; Expected results:
;   - %dead_arith1, %dead_arith2, %dead_cmp should be ELIMINATED
;   - store instructions should be PRESERVED  (side effect: memory write)
;   - call to @printf should be PRESERVED      (side effect: I/O)
;   - %ptr (alloca) should be PRESERVED        (used by store and load)
;   - %loaded should be PRESERVED              (used by ret)
;   - ret should be PRESERVED                  (terminator)
;
; Run:
;   opt -load-pass-plugin ./build/DeadInstructionEliminator.so \
;       -passes="dead-instruction-eliminator" -S tests/test3.ll
; ═══════════════════════════════════════════════════════════════════════════

declare i32 @printf(i8*, ...)

define i32 @mixed_live_dead(i32 %n) {
entry:
  ; ── Alloca + store (side effects → must keep) ───────────────────────
  %ptr = alloca i32
  store i32 %n, i32* %ptr

  ; ── Dead arithmetic (results unused) ────────────────────────────────
  %dead_arith1 = add i32 %n, 42
  %dead_arith2 = mul i32 %dead_arith1, 7
  %dead_cmp    = icmp sgt i32 %dead_arith2, 100

  ; ── Live load (result used by ret) ──────────────────────────────────
  %loaded = load i32, i32* %ptr

  ; ── Side-effecting call (must keep) ─────────────────────────────────
  %fmt = getelementptr inbounds [4 x i8], [4 x i8]* @.str, i32 0, i32 0
  call i32 (i8*, ...) @printf(i8* %fmt, i32 %loaded)

  ret i32 %loaded
}

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00"
