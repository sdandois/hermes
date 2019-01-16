// RUN: (%hermes -enable-eval=true %s 2>&1 || true) | %FileCheck --match-full-lines %s

"use strict";

function foo () {
    arguments = 0;
//CHECK: {{.*}}invalid-args-eval.js:6:5: error: invalid assignment left-hand side
//CHECK-NEXT:     arguments = 0;
//CHECK-NEXT:     ^~~~~~~~~

    eval = 0;
//CHECK: {{.*}}invalid-args-eval.js:11:5: error: invalid assignment left-hand side
//CHECK-NEXT:     eval = 0;
//CHECK-NEXT:     ^~~~

    ++arguments;
//CHECK: {{.*}}invalid-args-eval.js:16:7: error: invalid operand in update operation
//CHECK-NEXT:     ++arguments;
//CHECK-NEXT:       ^~~~~~~~~

    eval++;
//CHECK: {{.*}}invalid-args-eval.js:21:5: error: invalid operand in update operation
//CHECK-NEXT:     eval++;
//CHECK-NEXT:     ^~~~

    arguments += 1;
//CHECK: {{.*}}invalid-args-eval.js:26:5: error: invalid assignment left-hand side
//CHECK-NEXT:     arguments += 1;
//CHECK-NEXT:     ^~~~~~~~~

    eval -= 1;
//CHECK: {{.*}}invalid-args-eval.js:31:5: error: invalid assignment left-hand side
//CHECK-NEXT:     eval -= 1;
//CHECK-NEXT:     ^~~~

}

function bar () {
    var arguments, eval;
//CHECK: {{.*}}invalid-args-eval.js:39:9: error: cannot declare 'arguments'
//CHECK-NEXT:     var arguments, eval;
//CHECK-NEXT:         ^~~~~~~~~
//CHECK: {{.*}}invalid-args-eval.js:39:20: error: cannot declare 'eval'
//CHECK-NEXT:     var arguments, eval;
//CHECK-NEXT:                    ^~~~

    var eval;
//CHECK: {{.*}}invalid-args-eval.js:47:9: error: cannot declare 'eval'
//CHECK-NEXT:     var eval;
//CHECK-NEXT:         ^~~~

}

function baz(arguments, eval) { }
//CHECK: {{.*}}invalid-args-eval.js:54:14: error: cannot declare 'arguments'
//CHECK-NEXT: function baz(arguments, eval) { }
//CHECK-NEXT:              ^~~~~~~~~
//CHECK: {{.*}}invalid-args-eval.js:54:25: error: cannot declare 'eval'
//CHECK-NEXT: function baz(arguments, eval) { }
//CHECK-NEXT:                         ^~~~

var v1 = function (arguments) { }
//CHECK: {{.*}}invalid-args-eval.js:62:20: error: cannot declare 'arguments'
//CHECK-NEXT: var v1 = function (arguments) { }
//CHECK-NEXT:                    ^~~~~~~~~

function arguments () {}
//CHECK: {{.*}}invalid-args-eval.js:67:10: error: cannot declare 'arguments'
//CHECK-NEXT: function arguments () {}
//CHECK-NEXT:          ^~~~~~~~~

var v2 = function arguments () {}
//CHECK: {{.*}}invalid-args-eval.js:72:19: error: cannot declare 'arguments'
//CHECK-NEXT: var v2 = function arguments () {}
//CHECK-NEXT:                   ^~~~~~~~~

var v3 = function eval () {}
//CHECK: {{.*}}invalid-args-eval.js:77:19: error: cannot declare 'eval'
//CHECK-NEXT: var v3 = function eval () {}
//CHECK-NEXT:                   ^~~~

for(var arguments = 0; arguments < 10; ) {}
//CHECK: {{.*}}invalid-args-eval.js:82:9: error: cannot declare 'arguments'
//CHECK-NEXT: for(var arguments = 0; arguments < 10; ) {}
//CHECK-NEXT:         ^~~~~~~~~

for(var eval = 0; eval < 10; ) {}
//CHECK: {{.*}}invalid-args-eval.js:87:9: error: cannot declare 'eval'
//CHECK-NEXT: for(var eval = 0; eval < 10; ) {}
//CHECK-NEXT:         ^~~~
