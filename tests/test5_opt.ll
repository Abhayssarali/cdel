; ModuleID = '/mnt/d/Downloads/cdlabel/tests/test5.ll'
source_filename = "/mnt/d/Downloads/cdlabel/tests/test5.ll"

define i32 @all_live(i32 %a, i32 %b) {
entry:
  %sum = add i32 %a, %b
  %prod = mul i32 %sum, %a
  %diff = sub i32 %prod, %b
  ret i32 %diff
}
