/**
 * @file Consumer.h
 * @brief Declaration of AST consumer for ODPH.
 */
#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"

namespace odph {

class Consumer : public clang::ASTConsumer {
public:
  Consumer(Ctx *C, clang::ast_matchers::MatchFinder *F);
  void HandleTranslationUnit(clang::ASTContext &Context) override;
private:
  Ctx *Ctx_{};
  clang::ast_matchers::MatchFinder *Finder_{};
};

} // namespace odph
