/**
 * @file MatchCB.h
 * @brief Declaration of AST matcher callback for ODPCPP.
 */
#pragma once

#include <memory>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"

namespace odpcpp {

/**
 * @class MatchCB
 * @brief Clang AST match callback that forwards matched nodes to the context.
 */
class MatchCB : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  /**
   * @brief Construct with a plugin context.
   * @param C Pointer to active context.
   */
  explicit MatchCB(Ctx *C) : Ctx_(C) {}

  /**
   * @brief Handle a matcher result and report to context.
   * @param Res Match result set.
   */
  void run(const clang::ast_matchers::MatchFinder::MatchResult &Res) override;

private:
  Ctx *Ctx_{}; ///< Non-owning context pointer
};

} // namespace odpcpp
