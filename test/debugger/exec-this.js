// RUN: %hdb %s < %s.debug | %FileCheck --match-full-lines %s
// REQUIRES: debugger

function foo() {
  function bar() {
    function baz() {
      debugger;
    }
    baz.call(3);
  }
  bar.call('asdf');
}

foo.call(10);

// CHECK: Break on 'debugger' statement in baz: {{.*}}:7:7
// CHECK-NEXT: 3
// CHECK-NEXT: 10
// CHECK-NEXT: Stepped to bar: {{.*}}:9:13
// CHECK-NEXT: asdf
// CHECK-NEXT: asdfasdf
// CHECK-NEXT: Stepped to foo: {{.*}}:11:11
// CHECK-NEXT: 10
// CHECK-NEXT: 13
// CHECK-NEXT: Continuing execution
