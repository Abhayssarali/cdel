; ═══════════════════════════════════════════════════════════════════════════
; TEST 5: All-Live (No Dead Code)
; ═══════════════════════════════════════════════════════════════════════════
;
; Description:
;   Every instruction's result is used.  The pass should report 0 dead
;   instructions and leave the IR completely unchanged.
;
; Expected results:
;   - 0 instructions eliminated
;   - Output IR should be identical to input IR
;
; Run:
;   opt -load-pass-plugin ./build/DeadInstructionEliminator.so \
;       -passes="dead-instruction-eliminator" -S tests/test5.ll
; ═══════════════════════════════════════════════════════════════════════════

define i32 @all_live(i32 %a, i32 %b) {
entry:
  %sum  = add i32 %a, %b       ; used by %prod
  %prod = mul i32 %sum, %a     ; used by %diff
  %diff = sub i32 %prod, %b    ; used by ret
  ret i32 %diff
}
