/**
 * @file Action.cpp
 * @brief Definition of the plugin FrontendAction for ODPH.
 */

#include "Action.h"
#include "Consumer.h"
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::ast_matchers;

namespace odph {

std::unique_ptr<ASTConsumer>
Action::CreateASTConsumer(CompilerInstance &CI, llvm::StringRef InFile) {
  llvm::errs() << "ODPH: START plugin\n"; llvm::errs().flush();
  if (Opt_.debugErrors) {
    const unsigned id = CI.getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error, "ODPH: START TU %0");
    auto DB = CI.getDiagnostics().Report(id);
    DB << InFile.str();
    llvm::errs() << "ODPH: START TU " << InFile << "\n"; llvm::errs().flush();
  }
  Ctx_ = std::make_unique<Ctx>(CI);
  Ctx_->setOptions(Opt_);
  CB_ = std::make_unique<MatchCB>(Ctx_.get());

  auto inMain = isExpansionInMainFile();
  // Count only top-level (non-nested) records and enums; allow within namespaces.
  auto notNestedRecord = unless(hasAncestor(cxxRecordDecl()));
  Finder_.addMatcher(cxxRecordDecl(isDefinition(), inMain, notNestedRecord).bind("cxxrec"), CB_.get());
  Finder_.addMatcher(enumDecl(isDefinition(), inMain, notNestedRecord).bind("enum"), CB_.get());
  // Typedefs and type aliases are countable; exclude ones nested inside records
  Finder_.addMatcher(typedefDecl(inMain, notNestedRecord).bind("typedef"), CB_.get());
  Finder_.addMatcher(typeAliasDecl(inMain, notNestedRecord).bind("typealias"), CB_.get());
  // Free function definitions only (exclude methods)
  // Free function declarations (not just definitions) are countable to enforce one-per-header
  Finder_.addMatcher(functionDecl(unless(isImplicit()), unless(cxxMethodDecl()), inMain).bind("func"), CB_.get());
  // Non-countable: method and field definitions should still have docs enforced
  Finder_.addMatcher(cxxMethodDecl(isDefinition(), unless(isImplicit()), inMain).bind("method"), CB_.get());
  Finder_.addMatcher(fieldDecl(inMain).bind("field"), CB_.get());

  return std::make_unique<Consumer>(Ctx_.get(), &Finder_);
}

bool Action::ParseArgs(const CompilerInstance &, const std::vector<std::string> &args) {
  Options O;
  for (const auto &a : args) {
    if (a == "no-header") O.enforceHeader = false;
    if (a == "no-brief") O.requireBrief = false;
    if (a == "no-doc") O.enforceDoc = false;
    if (a == "print-constructs") O.printConstructs = true;
    if (a == "debug-errors") O.debugErrors = true;
  }
  Opt_ = O;
  return true;
}

static FrontendPluginRegistry::Add<Action>
    X("one-definition-per-header", "enforce one construct per header with docstrings (AST matchers)");

} // namespace odph
