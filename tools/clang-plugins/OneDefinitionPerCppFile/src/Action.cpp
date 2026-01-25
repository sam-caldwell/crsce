/**
 * @file Action.cpp
 * @brief Definition of the plugin FrontendAction for ODPCPP.
 */

#include "Action.h"
#include "Consumer.h"
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::ast_matchers;

namespace odpcpp {

std::unique_ptr<ASTConsumer>
Action::CreateASTConsumer(CompilerInstance &CI, llvm::StringRef InFile) {
  llvm::errs() << "ODPCPP: START plugin\n"; llvm::errs().flush();
  if (Opt_.debugErrors) {
    // Emit an early diagnostic and stderr line to verify control path in replay
    const unsigned id = CI.getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error, "ODPCPP: START TU %0");
    auto DB = CI.getDiagnostics().Report(id);
    DB << InFile.str();
    llvm::errs() << "ODPCPP: START TU " << InFile << "\n"; llvm::errs().flush();
  }
  Ctx_ = std::make_unique<Ctx>(CI);
  Ctx_->setOptions(Opt_);
  CB_ = std::make_unique<MatchCB>(Ctx_.get());

  auto inMain = isExpansionInMainFile();
  // Free functions (exclude methods)
  Finder_.addMatcher(functionDecl(isDefinition(), unless(cxxMethodDecl()), inMain).bind("freeFunc"), CB_.get());
  // C++ members (doc check only)
  Finder_.addMatcher(cxxConstructorDecl(isDefinition(), inMain).bind("ctor"), CB_.get());
  Finder_.addMatcher(cxxDestructorDecl(isDefinition(), inMain).bind("dtor"), CB_.get());
  Finder_.addMatcher(cxxMethodDecl(isDefinition(), inMain).bind("method"), CB_.get());
  // Top-level types/aliases/enums (countable)
  Finder_.addMatcher(cxxRecordDecl(isDefinition(), inMain).bind("cxxrec"), CB_.get());
  Finder_.addMatcher(enumDecl(isDefinition(), inMain).bind("enum"), CB_.get());
  Finder_.addMatcher(typedefDecl(inMain).bind("typedef"), CB_.get());
  Finder_.addMatcher(typeAliasDecl(inMain).bind("typealias"), CB_.get());
  // Top-level variable definitions (e.g., constant tables)
  Finder_.addMatcher(varDecl(isDefinition(), hasGlobalStorage(), inMain).bind("var"), CB_.get());
  // Class/struct fields (definitions inside TU). Enforce docs; do not count toward one-per-cpp.
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
    X("OneDefinitionPerCppFile", "enforce one construct per source with docstrings (AST matchers)");

} // namespace odpcpp
