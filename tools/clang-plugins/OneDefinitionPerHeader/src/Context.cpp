/**
 * @file Context.cpp
 * @brief Definitions for ODPH plugin context, options, and checks.
 */

#include "Context.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

using namespace clang;

namespace odph {

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

  if (Opt_.enforceDoc && countable) {
    DocRequirements req{};
    req.requireBlock = true;
    // ODPH only requires a brief Doxygen block for the construct; no @name/@param/@return.
    req.requireName = false;
    req.requireBrief = Opt_.requireBrief;
    req.requireParams = false;
    req.requireReturn = false;
    std::string missing;
    bool ok = checkDocForDecl(*CI_, D, req, missing);
    if (!ok) {
      const unsigned id = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "%0");
      std::string msg = "definition must be immediately preceded by a Doxygen block";
      if (req.requireBrief) msg += " with @brief";
      DE().Report(Loc, id) << msg;
      HadError_ = true;
      llvm::errs() << "ODPH: doc MISSING: " << msg << " for "
                   << C.kind << ' ' << C.qname << " (line " << C.line << ")\n";
    }
    if (Opt_.printConstructs) {
      const unsigned wid = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Warning, "%0");
      std::string msg = std::string("ODPH: ") + C.kind + " " + C.qname
                        + " at line " + std::to_string(C.line) + (ok ? ": doc OK" : ": doc MISSING: ") + missing;
      DE().Report(Loc, wid) << msg;
    }
  }

  Constructs_.push_back(std::move(C));
}

void Ctx::finalizeTU() {
  auto &SMgr = SM();
  FileID FID = SMgr.getMainFileID();
  if (FID.isInvalid()) return;
  SourceLocation FL = SMgr.getLocForStartOfFile(FID);
  llvm::StringRef Path = SMgr.getFilename(FL);
  if (!Path.ends_with(".h")) return;

  bool headerOk = true;
  if (Opt_.enforceHeader) {
    headerOk = checkHeaderDoc(Path, FID);
  }

  unsigned cnt = 0;
  for (const auto &c : Constructs_) if (c.countable) ++cnt;

  if (cnt != 1U) {
    const unsigned id = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "exactly one construct must be defined per header; found %0");
    auto DB = DE().Report(FL, id);
    DB << cnt;
    HadError_ = true;
    const unsigned noteId = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Note, "%0");
    for (const auto &c : Constructs_) {
      std::string msg = std::string("construct: ") + c.kind + " " + c.qname
                        + " (line " + std::to_string(c.line) + ")";
      DE().Report(FL, noteId) << msg;
    }
  }

  const unsigned sid = CI_->getDiagnostics().getCustomDiagID(
      Opt_.debugErrors ? DiagnosticsEngine::Error : DiagnosticsEngine::Warning, "%0");
  std::string header = std::string("ODPH: constructs=") + std::to_string(Constructs_.size())
                       + ", countable=" + std::to_string(cnt)
                       + ", header=" + (headerOk ? "OK" : "FAIL");
  DE().Report(FL, sid) << header;
  if (Opt_.printConstructs) {
    for (const auto &c : Constructs_) {
      std::string msg = std::string("construct: ") + c.kind + " " + c.qname
                        + " (line " + std::to_string(c.line) + ")";
      DE().Report(FL, sid) << msg;
    }
  }
  // Always print stderr summary
  llvm::errs() << (HadError_ ? "ODPH: FAIL file: " : "ODPH: OK file: ")
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

static bool containsTag(llvm::StringRef txt, llvm::StringRef tag) {
  return txt.contains(tag);
}

static bool containsParamFor(llvm::StringRef txt, const std::string &name) {
  if (name.empty()) return false;
  std::string pat1 = std::string("@param ") + name;
  if (txt.contains(pat1)) return true;
  size_t pos = 0;
  while (true) {
    size_t p = txt.find("@param[", pos);
    if (p == llvm::StringRef::npos) break;
    size_t rb = txt.find(']', p + 7);
    if (rb == llvm::StringRef::npos) break;
    size_t sp = rb + 1;
    while (sp < txt.size() && isspace(static_cast<unsigned char>(txt[sp]))) sp++;
    if (sp < txt.size()) {
      if (txt.drop_front(sp).starts_with(name)) return true;
    }
    pos = rb + 1;
  }
  return false;
}

bool Ctx::checkDocForDecl(CompilerInstance &CI, const Decl *D,
                          const DocRequirements &Req,
                          std::string &missingTags) {
  missingTags.clear();
  auto &Ctx = CI.getASTContext();
  const RawComment *RC = Ctx.getRawCommentForDeclNoCache(D);
  if (!RC) { missingTags = "no Doxygen block"; return false; }
  // Enforce that the comment is adjacent to the declaration (not just a file header comment).
  auto &SMgr = CI.getSourceManager();
  // If the comment begins at the very first line of the file, treat it as the
  // file header block, not the construct documentation.
  const unsigned rcBeginLine = SMgr.getSpellingLineNumber(
      SMgr.getFileLoc(RC->getSourceRange().getBegin()));
  if (rcBeginLine == 1U) {
    missingTags = "no Doxygen block";
    return false;
  }
  const unsigned declLine = SMgr.getSpellingLineNumber(SMgr.getFileLoc(D->getBeginLoc()));
  const unsigned rcEndLine = SMgr.getSpellingLineNumber(SMgr.getFileLoc(RC->getSourceRange().getEnd()));
  // Allow one intervening line (e.g., a 'template<...>' line) between comment and declaration.
  if (declLine > rcEndLine + 2) {
    missingTags = "no Doxygen block";
    return false;
  }
  const auto RCText = RC->getRawText(CI.getSourceManager());
  if (RCText.empty()) { missingTags = "empty Doxygen block"; return false; }
  if (Req.requireBlock && !RCText.ltrim().starts_with("/**")) {
    missingTags += (missingTags.empty() ? "" : ", "); missingTags += "block-style '/**'";
  }
  if (Req.requireName && !containsTag(RCText, "@name")) {
    missingTags += (missingTags.empty() ? "" : ", "); missingTags += "@name";
  }
  if (Req.requireBrief && !containsTag(RCText, "@brief")) {
    missingTags += (missingTags.empty() ? "" : ", "); missingTags += "@brief";
  }
  if (Req.requireReturn && !containsTag(RCText, "@return")) {
    missingTags += (missingTags.empty() ? "" : ", "); missingTags += "@return";
  }
  if (Req.requireParams) {
    if (const auto *FD = llvm::dyn_cast<FunctionDecl>(D)) {
      for (unsigned i = 0; i < FD->getNumParams(); ++i) {
        const ParmVarDecl *P = FD->getParamDecl(i);
        const std::string nm = P->getNameAsString();
        if (nm.empty() || !containsParamFor(RCText, nm)) {
          missingTags += (missingTags.empty() ? "" : ", ");
          missingTags += std::string("@param ") + (nm.empty() ? "<unnamed>" : nm);
        }
      }
    }
  }
  return missingTags.empty();
}

bool Ctx::checkHeaderDoc(llvm::StringRef Path, FileID FID) {
  auto &SMgr = SM();
  bool Invalid = false;
  llvm::StringRef C = SMgr.getBufferData(FID, &Invalid);
  if (Invalid) return false;
  bool startsWithBlock = C.starts_with("/**");
  bool hasEnd = C.contains("*/");
  // Limit header tag checks to the leading header block only.
  llvm::StringRef H = C;
  if (hasEnd) {
    size_t endPos = C.find("*/");
    H = C.take_front(endPos + 2);
  }
  // Treat '@file' present only when it appears in a conventional header line (e.g., ' * @file <name>').
  bool hasFile = H.contains("\n * @file ") || H.contains("\n* @file ");
  bool hasBrief = H.contains("\n * @brief") || H.contains("\n* @brief");
  bool hasAuthor = C.contains("Sam Caldwell");
  bool hasLicense = C.contains("LICENSE.txt");
  bool headerOk = startsWithBlock && hasEnd && hasFile && hasBrief && hasAuthor && hasLicense;
  if (!headerOk) {
    const unsigned id = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "header must start with Doxygen block containing @file/@brief and copyright");
    DE().Report(SMgr.getLocForStartOfFile(FID), id);
    HadError_ = true;
    // Provide detailed notes to aid debugging, mirroring ODPCPP
    const unsigned noteId = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Note, "%0");
    if (!startsWithBlock) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '/**' at line 1";
    if (!hasEnd) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "unterminated header block (missing '*/')";
    if (!hasFile) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '@file <basename>' tag";
    if (!hasBrief) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing '@brief' line in header";
    if (!hasAuthor) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing author marker ('Sam Caldwell')";
    if (!hasLicense) DE().Report(SMgr.getLocForStartOfFile(FID), noteId) << "missing license marker ('LICENSE.txt')";
  }
  llvm::SmallString<256> base = llvm::sys::path::filename(Path);
  std::string expect = std::string("@file ") + base.str().str();
  if (!C.contains(expect)) {
    const unsigned id2 = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "header @file tag must match current file basename (%0)");
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

clang::DiagnosticsEngine &Ctx::DE() { return CI_->getDiagnostics(); }

} // namespace odph
