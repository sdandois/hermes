#ifndef HERMES_SUPPORT_SCOPECHAIN_H
#define HERMES_SUPPORT_SCOPECHAIN_H

#include "llvm/ADT/ArrayRef.h"

#include <vector>

namespace hermes {
using llvm::StringRef;

/// A ScopeChainItem represents variables available in a scope.
struct ScopeChainItem {
  /// List of variables in this function.
  std::vector<StringRef> variables;
};
/// A ScopeChain is a sequence of nested ScopeChainItems, from innermost to
/// outermost scopes.
struct ScopeChain {
  /// Functions on the stack. Innermost (direct parent) is 0.
  std::vector<ScopeChainItem> functions;
};

} // namespace hermes

#endif // HERMES_SUPPORT_SCOPECHAIN_H
