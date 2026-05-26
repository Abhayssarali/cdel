; ModuleID = '/mnt/d/Downloads/cdlabel/tests/test1.ll'
source_filename = "/mnt/d/Downloads/cdlabel/tests/test1.ll"

define i32 @simple_dead(i32 %a, i32 %b) {
entry:
  %live1 = add i32 %a, %b
  %live2 = mul i32 %live1, 2
  ret i32 %live2
}
