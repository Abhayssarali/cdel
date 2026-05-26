; ═══════════════════════════════════════════════════════════════════════════
; TEST 4: PHI Nodes and Control Flow
; ═══════════════════════════════════════════════════════════════════════════
;
; Description:
;   Tests dead PHI nodes across basic block boundaries.  The function has
;   a diamond-shaped CFG with a dead PHI node in the merge block.
;
; Expected results:
;   - %dead_phi should be ELIMINATED  (unused PHI)
;   - %dead_left, %dead_right are ELIMINATED (feed only the dead PHI)
;   - %live_phi should be PRESERVED   (used by ret)
;   - All branches/terminators PRESERVED
;
; Run:
;   opt -load-pass-plugin ./build/DeadInstructionEliminator.so \
;       -passes="dead-instruction-eliminator" -S tests/test4.ll
; ═══════════════════════════════════════════════════════════════════════════

define i32 @phi_dead(i32 %x, i1 %cond) {
entry:
  br i1 %cond, label %left, label %right

left:
  %live_left  = add i32 %x, 10
  %dead_left  = mul i32 %x, 99       ; dead: only feeds dead_phi
  br label %merge

right:
  %live_right = sub i32 %x, 10
  %dead_right = add i32 %x, 77       ; dead: only feeds dead_phi
  br label %merge

merge:
  %live_phi = phi i32 [ %live_left, %left ], [ %live_right, %right ]
  %dead_phi = phi i32 [ %dead_left, %left ], [ %dead_right, %right ]  ; dead

  ret i32 %live_phi
}
