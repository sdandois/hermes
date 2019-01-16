// RUN: %hermes -target=HBC -dump-bytecode -pretty-disassemble=false -O %s | %FileCheck --match-full-lines %s

function fooNS() {
  return this;
}

//CHECK-LABEL: Function<fooNS>(1 params, 1 registers, 0 symbols):
//CHECK-NEXT: [@ 0] LoadThisNS 0<Reg8>
//CHECK-NEXT: [@ 2] Ret 0<Reg8>

function foo() {
  "use strict";
  return this;
}

//CHECK-LABEL: Function<foo>(1 params, 1 registers, 0 symbols):
//CHECK-NEXT: [@ 0] LoadParam 0<Reg8>, 0<UInt8>
//CHECK-NEXT: [@ 3] Ret 0<Reg8>
