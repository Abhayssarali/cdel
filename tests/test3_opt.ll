; ModuleID = '/mnt/d/Downloads/cdlabel/tests/test3.ll'
source_filename = "/mnt/d/Downloads/cdlabel/tests/test3.ll"

@.str = private unnamed_addr constant [4 x i8] c"%d\0A\00"

declare i32 @printf(ptr, ...)

define i32 @mixed_live_dead(i32 %n) {
entry:
  %ptr = alloca i32, align 4
  store i32 %n, ptr %ptr, align 4
  %loaded = load i32, ptr %ptr, align 4
  %fmt = getelementptr inbounds [4 x i8], ptr @.str, i32 0, i32 0
  %0 = call i32 (ptr, ...) @printf(ptr %fmt, i32 %loaded)
  ret i32 %loaded
}
