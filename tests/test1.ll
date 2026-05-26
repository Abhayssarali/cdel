; ═══════════════════════════════════════════════════════════════════════════
; TEST 1: Simple Dead Instructions
; ═══════════════════════════════════════════════════════════════════════════
;
; Description:
;   This test contains arithmetic instructions whose results are never used.
;   The DIE pass should eliminate all dead instructions while preserving the
;   return value computation.
;
; Expected results:
;   - %dead1, %dead2, %dead3 should be ELIMINATED  (unused results)
;   - %live1, %live2 should be PRESERVED             (used in return)
;   - ret instruction should be PRESERVED             (terminator)
;
; Run:
;   opt -load-pass-plugin ./build/DeadInstructionEliminator.so \
;       -passes="dead-instruction-eliminator" -S tests/test1.ll
; ═══════════════════════════════════════════════════════════════════════════

define i32 @simple_dead(i32 %a, i32 %b) {
entry:
  ; ── Dead instructions (results never used) ───────────────────────────
  %dead1 = add i32 %a, 10        ; dead: result unused
  %dead2 = mul i32 %b, 3         ; dead: result unused
  %dead3 = sub i32 %dead1, %dead2 ; dead: result unused (uses dead1, dead2)

  ; ── Live instructions (result flows to return) ───────────────────────
  %live1 = add i32 %a, %b        ; live: used by %live2
  %live2 = mul i32 %live1, 2     ; live: used by ret

  ret i32 %live2
}
