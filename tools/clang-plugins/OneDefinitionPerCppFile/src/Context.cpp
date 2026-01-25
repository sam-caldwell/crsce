/**
 * @file Context.cpp
 * @brief Definitions for plugin context, options, and construct checks.
 */

#include "Context.h"

#include <clang/AST/RawCommentList.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;

namespace odpcpp {

Ctx::Ctx(CompilerInstance &CI) : CI_(&CI) {}

void Ctx::setOptions(Options O) { Opt_ = std::move(O); }

CompilerInstance &Ctx::ci() { return *CI_; }

void Ctx::setASTContext(ASTContext &C) {
  AST_ = &C;
  SM_ = &C.getSourceManager();
}

void Ctx::addConstruct(const Decl *D, llvm::StringRef kind, bool countable) {
  auto &SMgr = SM();
  SourceLocation Loc = SMgr.getFileLoc(D->getBeginLoc());
  if (!SMgr.isWrittenInMainFile(Loc)) return;

  ConstructInfo C;
  C.line = SMgr.getSpellingLineNumber(Loc);
  C.kind = kind.str();
  C.qname = getQualifiedName(D);
  C.countable = countable;

  if (Opt_.enforceDoc) {
    bool ok = hasDocBrief(*CI_, D, Opt_.requireBrief);
    if (!ok) {
      const unsigned id = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "definition must be immediately preceded by a Doxygen block (/** ... */) with @brief");
      DE().Report(Loc, id);
      HadError_ = true;
      // Always emit a plain stderr line to aid CI triage
      llvm::errs() << "ODPCPP: doc MISSING for " << C.kind << ' ' << C.qname
                   << " (line " << C.line << ")\n";
    }
    if (Opt_.printConstructs) {
      const unsigned wid = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Warning, "%0");
      std::string msg = std::string("ODPCPP: ") + C.kind + " " + C.qname +
                        " at line " + std::to_string(C.line) + (ok ? ": doc OK" : ": doc MISSING");
      DE().Report(Loc, wid) << msg;
    }
  }

  Constructs_.push_back(std::move(C));
}

void Ctx::finalizeTU() {
  auto &SMgr2 = SM();
  FileID FID = SMgr2.getMainFileID();
  if (FID.isInvalid()) return;
  SourceLocation FL = SMgr2.getLocForStartOfFile(FID);
  llvm::StringRef Path = SMgr2.getFilename(FL);
  if (!Path.ends_with(".cpp")) return;

  const bool isSrc = Path.contains("/src/");
  const bool isCmd = Path.contains("/cmd/");

  bool headerOk = true;
  if (Opt_.enforceHeader && (isSrc || isCmd)) {
    headerOk = checkHeaderDoc(Path, FID);
  }

  unsigned cnt = 0;
  for (const auto &c : Constructs_) if (c.countable) ++cnt;

  if (isSrc && cnt != 1U) {
    const unsigned id = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "exactly one construct must be defined per source file; found %0");
    auto DB = DE().Report(FL, id);
    DB << cnt;
    HadError_ = true;
    const unsigned noteId = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Note, "%0");
    for (const auto &c : Constructs_) {
      std::string msg = std::string("construct: ") + c.kind + " " + c.qname +
                        " (line " + std::to_string(c.line) + ")";
      DE().Report(FL, noteId) << msg;
    }
  }

  if (isSrc || isCmd || Opt_.printConstructs) {
    const unsigned sid = CI_->getDiagnostics().getCustomDiagID(
        Opt_.debugErrors ? DiagnosticsEngine::Error : DiagnosticsEngine::Warning, "%0");
    std::string header = std::string("ODPCPP: constructs=") + std::to_string(Constructs_.size()) +
                         ", countable=" + std::to_string(cnt) +
                         ", header=" + (headerOk ? "OK" : "FAIL");
    DE().Report(FL, sid) << header;
    if (Opt_.printConstructs) {
      for (const auto &c : Constructs_) {
        std::string msg = std::string("construct: ") + c.kind + " " + c.qname +
                          " (line " + std::to_string(c.line) + ")";
        DE().Report(FL, sid) << msg;
      }
    }
  }
  // Explicit note when no countable constructs found under src
  if (isSrc && cnt == 0U) {
    const unsigned nid = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Note, "no countable constructs matched in source file");
    DE().Report(FL, nid);
    llvm::errs() << "ODPCPP: FAIL file: " << Path << " (no constructs matched)\n";
  }
  // Always print a stderr summary to aid CI triage (even if no errors)
  llvm::errs() << (HadError_ ? "ODPCPP: FAIL file: " : "ODPCPP: OK file: ")
               << Path << " constructs=" << Constructs_.size()
               << " countable=" << cnt << " header=" << (headerOk ? "OK" : "FAIL") << "\n";
  if (Opt_.printConstructs || HadError_) {
    for (const auto &c : Constructs_) {
      llvm::errs() << "  construct: " << c.kind << ' ' << c.qname << " (line " << c.line << ")\n";
    }
  }
}

// static
std::string Ctx::getQualifiedName(const Decl *D) {
  if (const auto *ND = llvm::dyn_cast<NamedDecl>(D)) {
    return ND->getQualifiedNameAsString();
  }
  return "<unnamed>";
}

// static
bool Ctx::hasDocBrief(CompilerInstance &CI, const Decl *D, bool requireBrief) {
  auto &Ctx = CI.getASTContext();
  const RawComment *RC = Ctx.getRawCommentForDeclNoCache(D);
  if (!RC) return false;
  const auto RCText = RC->getRawText(CI.getSourceManager());
  if (RCText.empty()) return false;
  if (!requireBrief) return true;
  return RCText.contains("@brief");
}

bool Ctx::checkHeaderDoc(llvm::StringRef Path, FileID FID) {
  auto &SMgr = SM();
  bool Invalid = false;
  llvm::StringRef C = SMgr.getBufferData(FID, &Invalid);
  if (Invalid) return false;
  bool startsWithBlock = C.starts_with("/**");
  bool hasEnd = C.contains("*/");
  bool hasFile = C.contains("@file ");
  bool hasBrief = C.contains("@brief");
  bool hasAuthor = C.contains("Sam Caldwell");
  bool hasLicense = C.contains("LICENSE.txt");
  bool headerOk = startsWithBlock && hasEnd && hasFile && hasBrief && hasAuthor && hasLicense;
  if (!headerOk) {
    const unsigned id = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "source must start with Doxygen block containing @file/@brief and copyright");
    DE().Report(SMgr.getLocForStartOfFile(FID), id);
    HadError_ = true;
    const unsigned noteId = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Note, "%0");
    if (!startsWithBlock) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '/**' at line 1";
    if (!hasEnd) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "unterminated header block (missing '*/')";
    if (!hasFile) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '@file <basename>' tag";
    if (!hasBrief) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '@brief' line in header";
    if (!hasAuthor) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing author marker ('Sam Caldwell')";
    if (!hasLicense) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing license marker ('LICENSE.txt')";
  }
  // @file must match basename
  llvm::SmallString<256> base = llvm::sys::path::filename(Path);
  std::string expect = std::string("@file ") + base.str().str();
  if (!C.contains(expect)) {
    const unsigned id2 = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "source @file tag must match current file basename (%0)");
    auto DB = DE().Report(SMgr.getLocForStartOfFile(FID), id2);
    DB << base.str().str();
    headerOk = false;
    HadError_ = true;
  }
  return headerOk;
}

clang::SourceManager &Ctx::SM() {
  if (SM_) return *SM_;
  return CI_->getSourceManager();
}

clang::DiagnosticsEngine &Ctx::DE() {
  return CI_->getDiagnostics();
}

} // namespace odpcpp
