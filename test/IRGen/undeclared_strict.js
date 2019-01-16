// RUN: %hermes -dump-ir -strict %s 2>&1 | %FileCheck %s --match-full-lines

var x = y;
//CHECK: {{.*}}undeclared_strict.js:3:9: warning: the variable "y" was not declared in function "global"
//CHECK-NEXT: var x = y;
//CHECK-NEXT:         ^

//CHECK: function global()
//CHECK-NEXT: frame = [], globals = [x]
//CHECK-NEXT: %BB0:
//CHECK-NEXT:   %0 = AllocStackInst $?anon_0_ret
//CHECK-NEXT:   %1 = StoreStackInst undefined : undefined, %0
//CHECK-NEXT:   %2 = TryLoadGlobalPropertyInst globalObject : object, "y" : string
//CHECK-NEXT:   %3 = StorePropertyInst %2, globalObject : object, "x" : string
//CHECK-NEXT:   %4 = LoadStackInst %0
//CHECK-NEXT:   %5 = ReturnInst %4
//CHECK-NEXT: function_end
