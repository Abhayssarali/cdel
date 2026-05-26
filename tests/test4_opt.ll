; ModuleID = '/mnt/d/Downloads/cdlabel/tests/test4.ll'
source_filename = "/mnt/d/Downloads/cdlabel/tests/test4.ll"

define i32 @phi_dead(i32 %x, i1 %cond) {
entry:
  br i1 %cond, label %left, label %right

left:                                             ; preds = %entry
  %live_left = add i32 %x, 10
  br label %merge

right:                                            ; preds = %entry
  %live_right = sub i32 %x, 10
  br label %merge

merge:                                            ; preds = %right, %left
  %live_phi = phi i32 [ %live_left, %left ], [ %live_right, %right ]
  ret i32 %live_phi
}
