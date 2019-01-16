// RUN: %hermes -lazy %s | %FileCheck --match-full-lines %s

function foo() {
    break;
    /* Some text to pad out the function so that it won't be eagerly compiled
     * for being too short. Lorem ipsum dolor sit amet, consectetur adipiscing
     * elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     */
}

function bar() {
    foo();
    /* Some text to pad out the function so that it won't be eagerly compiled
     * for being too short. Lorem ipsum dolor sit amet, consectetur adipiscing
     * elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
     */
}

print("main");
// CHECK: main

try {
    bar();
} catch(e) {
    print("caught", e);
}
// CHECK-NEXT: caught SyntaxError: 4:4:'break' not within a loop or a switch

print("end");
// CHECK-NEXT: end
