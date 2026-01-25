/**
 * @file Consumer.h
 * @brief Declaration of AST consumer for ODPCPP.
 */
#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"

namespace odpcpp {

/**
 * @class Consumer
 * @brief Runs matchers over the TU and finalizes plugin context.
 */
class Consumer : public clang::ASTConsumer {
public:
  /**
   * @brief Construct with context and match finder.
   * @param C Plugin context pointer.
   * @param F MatchFinder pointer with registered matchers.
   */
  Consumer(Ctx *C, clang::ast_matchers::MatchFinder *F);

  /**
   * @brief Execute matchers and finalize TU checks.
   * @param Context AST context.
   */
  void HandleTranslationUnit(clang::ASTContext &Context) override;

private:
  Ctx *Ctx_{};                                    ///< Non-owning context
  clang::ast_matchers::MatchFinder *Finder_{};    ///< Non-owning finder
};

} // namespace odpcpp

