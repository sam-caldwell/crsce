/**
 * @file MatchCB.cpp
 * @brief Definition of AST matcher callback for ODPCPP.
 */

#include "MatchCB.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::ast_matchers;

namespace odpcpp {

void MatchCB::run(const MatchFinder::MatchResult &Res) {
  if (const auto *FD = Res.Nodes.getNodeAs<FunctionDecl>("freeFunc")) {
    llvm::errs() << "ODPCPP: MATCH function " << FD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(FD, "function", /*countable*/true);
    return;
  }
  if (const auto *CD = Res.Nodes.getNodeAs<CXXConstructorDecl>("ctor")) {
    llvm::errs() << "ODPCPP: MATCH ctor " << CD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(CD, "ctor", /*countable*/true);
    return;
  }
  if (const auto *DD = Res.Nodes.getNodeAs<CXXDestructorDecl>("dtor")) {
    llvm::errs() << "ODPCPP: MATCH dtor " << DD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(DD, "dtor", /*countable*/true);
    return;
  }
  if (const auto *MD = Res.Nodes.getNodeAs<CXXMethodDecl>("method")) {
    // Avoid double-counting: constructors/destructors are handled separately
    if (llvm::isa<CXXConstructorDecl>(MD) || llvm::isa<CXXDestructorDecl>(MD)) {
      return;
    }
    llvm::errs() << "ODPCPP: MATCH method " << MD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(MD, "method", /*countable*/true);
    return;
  }
  if (const auto *RD = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxrec")) {
    llvm::errs() << "ODPCPP: MATCH record " << RD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(RD, "record", /*countable*/true);
    return;
  }
  if (const auto *ED = Res.Nodes.getNodeAs<EnumDecl>("enum")) {
    llvm::errs() << "ODPCPP: MATCH enum " << ED->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(ED, "enum", /*countable*/true);
    return;
  }
  if (const auto *TD = Res.Nodes.getNodeAs<TypedefDecl>("typedef")) {
    llvm::errs() << "ODPCPP: MATCH typedef " << TD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(TD, "typedef", /*countable*/true);
    return;
  }
  if (const auto *TA = Res.Nodes.getNodeAs<TypeAliasDecl>("typealias")) {
    llvm::errs() << "ODPCPP: MATCH type-alias " << TA->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(TA, "type-alias", /*countable*/true);
    return;
  }
  if (const auto *VD = Res.Nodes.getNodeAs<VarDecl>("var")) {
    // Count only file/namespace-scope definitions
    if (VD->isFileVarDecl() && VD->isThisDeclarationADefinition()) {
      llvm::errs() << "ODPCPP: MATCH var " << VD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
      Ctx_->addConstruct(VD, "var", /*countable*/true);
      return;
    }
  }
  if (const auto *FD = Res.Nodes.getNodeAs<FieldDecl>("field")) {
    // Enforce docs on class/struct fields when the record is in this TU. Not countable.
    llvm::errs() << "ODPCPP: MATCH field " << FD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(FD, "field", /*countable*/false);
    return;
  }
}

} // namespace odpcpp
