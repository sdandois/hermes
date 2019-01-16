// RUN: %hdb %s < %s.debug | %FileCheck --match-full-lines %s
// REQUIRES: debugger

function eventuallyThrows(x) {
  if (x <= 0)
    throw new Error("I frew up.");
  eventuallyThrows(x-1);
}

function callme() {
  print("Hello");
  debugger;
  print("Goodbye");
}

callme();

// CHECK: Hello
// CHECK-NEXT: Break on 'debugger' statement in callme: {{.*}}:12:3
// CHECK-NEXT: Exception: SyntaxError: 1:6:';' expected
// CHECK-NEXT: Thrown value is: { message: 1:6:';' expected }
// CHECK-NEXT: Next
// CHECK-NEXT: Exception: Error: I frew up.
// CHECK-NEXT:   0: eventuallyThrows: {{.*}}:6:20
// CHECK-NEXT:   1: eventuallyThrows: {{.*}}:7:19
// CHECK-NEXT:   2: eventuallyThrows: {{.*}}:7:19
// CHECK-NEXT:   3: eventuallyThrows: {{.*}}:7:19
// CHECK-NEXT:   4: eventuallyThrows: {{.*}}:7:19
// CHECK-NEXT:   5: eventuallyThrows: {{.*}}:7:19
// CHECK-NEXT:   6: eval: {{.*}}:1:18
// CHECK-NEXT:   7: (native)
// CHECK-NEXT:   8: global: {{.*}}:16:7
// CHECK-NEXT: Thrown value is: { message: I frew up. }
// CHECK-NEXT: Continuing execution
// CHECK-NEXT: Goodbye
