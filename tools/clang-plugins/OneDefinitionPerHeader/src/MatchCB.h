/**
 * @file MatchCB.h
 * @brief Declaration of AST matcher callback for ODPH.
 */
#pragma once

#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"

namespace odph {

class MatchCB : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  explicit MatchCB(Ctx *C) : Ctx_(C) {}
  void run(const clang::ast_matchers::MatchFinder::MatchResult &Res) override;
private:
  Ctx *Ctx_{};
};

} // namespace odph
