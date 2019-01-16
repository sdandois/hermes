#ifndef HERMES_AST_ESTREEVISITORS_H
#define HERMES_AST_ESTREEVISITORS_H

#include <map>
#include <string>

namespace hermes {
namespace ESTree {

struct TreeTrait {
  unsigned Count{0};
  unsigned MaxDepth{0};
  unsigned CurrDepth{0};

  explicit TreeTrait() = default;
  ~TreeTrait() = default;

  bool shouldVisit(ESTree::Node *V) {
    return true;
  }

  void enter(ESTree::Node *V) {
    Count += 1;
    CurrDepth += 1;
    MaxDepth = std::max(MaxDepth, CurrDepth);
  }
  void leave(ESTree::Node *V) {
    CurrDepth -= 1;
  }
};

} // namespace ESTree
} // namespace hermes

#endif
