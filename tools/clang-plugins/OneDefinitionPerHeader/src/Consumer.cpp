/**
 * @file Consumer.cpp
 * @brief Definition of AST consumer for ODPH.
 */

#include "Consumer.h"
#include <llvm/Support/raw_ostream.h>

using namespace clang;
using namespace clang::ast_matchers;

namespace odph {

Consumer::Consumer(Ctx *C, MatchFinder *F) : Ctx_(C), Finder_(F) {}

void Consumer::HandleTranslationUnit(ASTContext &Context) {
  // Bind AST context and source manager
  Ctx_->setASTContext(Context);
  auto &SM = Context.getSourceManager();
  auto FID = SM.getMainFileID();
  auto Path = SM.getFilename(SM.getLocForStartOfFile(FID));
  llvm::errs() << "ODPH: BEGIN TU " << Path << "\n"; llvm::errs().flush();
  Finder_->matchAST(Context);
  Ctx_->finalizeTU();
  llvm::errs() << "ODPH: END TU " << Path << "\n"; llvm::errs().flush();
}

} // namespace odph
