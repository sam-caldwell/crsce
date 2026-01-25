/**
 * @file Action.h
 * @brief Declaration of the plugin FrontendAction for ODPH.
 */
#pragma once

#include <memory>

#include <clang/Frontend/FrontendAction.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>

#include "Context.h"
#include "MatchCB.h"

namespace odph {

class Action : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, llvm::StringRef InFile) override;

  bool ParseArgs(const clang::CompilerInstance &CI, const std::vector<std::string> &args) override;

  ActionType getActionType() override { return ReplaceAction; }

private:
  Options Opt_{};
  std::unique_ptr<Ctx> Ctx_;
  std::unique_ptr<MatchCB> CB_;
  clang::ast_matchers::MatchFinder Finder_{};
};

} // namespace odph
