/**
 * @file Action.h
 * @brief Declaration of the plugin FrontendAction for ODPCPP.
 */
#pragma once

#include <memory>

#include <clang/Frontend/FrontendAction.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"
#include "MatchCB.h"

namespace odpcpp {

/**
 * @class Action
 * @brief Configures matchers, parses options, and wires up the consumer.
 */
class Action : public clang::PluginASTAction {
public:
  /**
   * @brief Create an AST consumer that runs matchers and finalizes context.
   */
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  /**
   * @brief Parse plugin arguments from -Xclang -plugin-arg-...
   */
  bool ParseArgs(const clang::CompilerInstance &CI, const std::vector<std::string> &args) override;

  /**
   * @brief Run before main; do not modify the source.
   */
  ActionType getActionType() override { return ReplaceAction; }

private:
  Options Opt_{};                                 ///< Effective options
  std::unique_ptr<Ctx> Ctx_;                      ///< Plugin context
  std::unique_ptr<MatchCB> CB_;                   ///< Match callback
  clang::ast_matchers::MatchFinder Finder_{};     ///< Match registry
};

} // namespace odpcpp
