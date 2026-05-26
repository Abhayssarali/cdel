; ModuleID = '/mnt/d/Downloads/cdlabel/tests/test2.ll'
source_filename = "/mnt/d/Downloads/cdlabel/tests/test2.ll"

define i32 @cascading_dead(i32 %x, i32 %y) {
entry:
  %result = add i32 %x, %y
  ret i32 %result
}
