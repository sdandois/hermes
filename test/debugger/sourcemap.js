// RUN: %hdb %s < %s.debug | %FileCheck --match-full-lines %s
// REQUIRES: debugger

//# sourceMappingURL=this_is_a_url

debugger;
// CHECK: Break on 'debugger' statement in global:{{.*}}
// CHECK: this_is_a_url
// CHECK-NEXT: Continuing execution
