// RUN: %hermes -non-strict -O -target=HBC %s | %FileCheck --match-full-lines %s

var propKeyEvaluated = false;
var obj = {};
var prop = {
  toString: function() {
    print("toString called");
    if (propKeyEvaluated) {
      throw new Error("toString called several times");
    }
    propKeyEvaluated = true;
    return "";
  }
};

obj[prop] = 0;
//CHECK: toString called

propKeyEvaluated = false;
print(obj[prop]);
//CHECK: toString called
//CHECK: 0

propKeyEvaluated = false;
delete obj[prop];
//CHECK: toString called

propKeyEvaluated = false;
Object.defineProperty(obj, prop, {value: 0});
//CHECK: toString called

propKeyEvaluated = false;
Object.getOwnPropertyDescriptor(obj, prop);
//CHECK: toString called

propKeyEvaluated = false;
print(obj.hasOwnProperty(prop));
//CHECK: toString called
//CHECK: true

propKeyEvaluated = false;
print(obj.propertyIsEnumerable(prop));
//CHECK: toString called
//CHECK: false

propKeyEvaluated = false;
print(prop in obj);
//CHECK: toString called
//CHECK: true
