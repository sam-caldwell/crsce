/**
 * @file MatchCB.cpp
 * @brief Definition of AST matcher callback for ODPH.
 */

#include "MatchCB.h"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::ast_matchers;

namespace odph {

void MatchCB::run(const MatchFinder::MatchResult &Res) {
  if (const auto *RD = Res.Nodes.getNodeAs<CXXRecordDecl>("cxxrec")) {
    llvm::errs() << "ODPH: MATCH record " << RD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(RD, "record", /*countable*/true);
    return;
  }
  if (const auto *ED = Res.Nodes.getNodeAs<EnumDecl>("enum")) {
    llvm::errs() << "ODPH: MATCH enum " << ED->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(ED, "enum", /*countable*/true);
    return;
  }
  if (const auto *TD = Res.Nodes.getNodeAs<TypedefDecl>("typedef")) {
    llvm::errs() << "ODPH: MATCH typedef " << TD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(TD, "typedef", /*countable*/true);
    return;
  }
  if (const auto *TA = Res.Nodes.getNodeAs<TypeAliasDecl>("typealias")) {
    llvm::errs() << "ODPH: MATCH type-alias " << TA->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(TA, "type-alias", /*countable*/true);
    return;
  }
  if (const auto *FD = Res.Nodes.getNodeAs<FunctionDecl>("func")) {
    llvm::errs() << "ODPH: MATCH function " << FD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(FD, "function", /*countable*/true);
    return;
  }
  if (const auto *VD = Res.Nodes.getNodeAs<VarDecl>("var")) {
    llvm::errs() << "ODPH: MATCH var " << VD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(VD, "var", /*countable*/true);
    return;
  }
  if (const auto *MD = Res.Nodes.getNodeAs<CXXMethodDecl>("method")) {
    llvm::errs() << "ODPH: MATCH method " << MD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(MD, "method", /*countable*/false);
    return;
  }
  if (const auto *FLD = Res.Nodes.getNodeAs<FieldDecl>("field")) {
    llvm::errs() << "ODPH: MATCH field " << FLD->getQualifiedNameAsString() << "\n"; llvm::errs().flush();
    Ctx_->addConstruct(FLD, "field", /*countable*/false);
    return;
  }
}

} // namespace odph
