#ifndef HERMES_VM_STACKFRAME_INLINE_H
#define HERMES_VM_STACKFRAME_INLINE_H

#include "hermes/VM/StackFrame.h"

namespace hermes {
namespace vm {

template <bool isConst>
inline Callable *StackFramePtrT<isConst>::getCalleeClosureUnsafe() const {
  return vmcast<Callable>(getCalleeClosureOrCBRef());
}

template <bool isConst>
inline Handle<Callable> StackFramePtrT<isConst>::getCalleeClosureHandleUnsafe()
    const {
  return Handle<Callable>::vmcast(&getCalleeClosureOrCBRef());
}

template <bool isConst>
typename StackFramePtrT<isConst>::QualifiedCB *
StackFramePtrT<isConst>::getCalleeCodeBlock() const {
  auto &ref = getCalleeClosureOrCBRef();
  if (ref.isObject()) {
    if (auto *func = dyn_vmcast<JSFunction>(ref))
      return func->getCodeBlock();
    else
      return nullptr;
  } else {
    return ref.template getNativePointer<CodeBlock>();
  }
}

template <bool isConst>
inline Callable *StackFramePtrT<isConst>::getCalleeClosure() const {
  return dyn_vmcast<Callable>(getCalleeClosureOrCBRef());
}

template <bool isConst>
inline Handle<Environment> StackFramePtrT<isConst>::getDebugEnvironmentHandle()
    const {
  return Handle<Environment>::vmcast_or_null(&getDebugEnvironmentRef());
}

template <bool isConst>
inline Environment *StackFramePtrT<isConst>::getDebugEnvironment() const {
  return vmcast_or_null<Environment>(getDebugEnvironmentRef());
}

} // namespace vm
} // namespace hermes

#endif // HERMES_VM_STACKFRAME_INLINE_H
