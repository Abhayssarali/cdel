; ═══════════════════════════════════════════════════════════════════════════
; TEST 2: Cascading Dead Instruction Chain
; ═══════════════════════════════════════════════════════════════════════════
;
; Description:
;   This test demonstrates the cascading nature of dead code elimination.
;   Instruction %e is unused.  This makes %d dead (its only user was %e).
;   That in turn makes %c dead (its only user was %d), and so on.
;   The entire chain %a → %b → %c → %d → %e should be eliminated.
;
; Expected results:
;   - %a, %b, %c, %d, %e should ALL be ELIMINATED  (cascading dead chain)
;   - %result should be PRESERVED                    (used by ret)
;   - ret should be PRESERVED                        (terminator)
;
; This demonstrates the power of the iterative worklist algorithm:
;   a single pass discovers and eliminates the entire dead chain.
;
; Run:
;   opt -load-pass-plugin ./build/DeadInstructionEliminator.so \
;       -passes="dead-instruction-eliminator" -S tests/test2.ll
; ═══════════════════════════════════════════════════════════════════════════

define i32 @cascading_dead(i32 %x, i32 %y) {
entry:
  ; ── Dead chain (entire chain is dead because %e is unused) ───────────
  %a = add i32 %x, 1         ; dead: only user is %b (which is dead)
  %b = mul i32 %a, 2         ; dead: only user is %c (which is dead)
  %c = sub i32 %b, %y        ; dead: only user is %d (which is dead)
  %d = add i32 %c, 100       ; dead: only user is %e (which is dead)
  %e = mul i32 %d, %d        ; dead: result unused ← elimination starts here

  ; ── Live computation ─────────────────────────────────────────────────
  %result = add i32 %x, %y   ; live: used by ret

  ret i32 %result
}
