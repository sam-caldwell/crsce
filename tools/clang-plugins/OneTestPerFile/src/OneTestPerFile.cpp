/**
 * @file OneTestPerFile.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
#if __has_include(<clang/AST/AST.h>)
#  include "ClangIncludes.h"
#  include <unordered_map>
#  include <memory>
#  include <llvm/ADT/StringRef.h>
#  include <clang/AST/ASTConsumer.h>
#  include <clang/AST/ASTContext.h>
#  include <clang/AST/Decl.h>
#  include <clang/AST/RecursiveASTVisitor.h>
#  include <clang/Frontend/FrontendAction.h>
#  include <clang/Frontend/FrontendPluginRegistry.h>
#  include <clang/Basic/IdentifierTable.h>
#  include <clang/Lex/Token.h>
#  include <clang/Lex/Preprocessor.h>
#  include <clang/Lex/PPCallbacks.h>
#  include <clang/Basic/SourceLocation.h>
#  include "llvm/Support/Path.h"
#else
#  include <string_view>
namespace clang { struct SourceLocation { [[nodiscard]] bool isValid() const { return false; } }; struct FileID { unsigned _{}; }; }
#endif
/*
 * Brief: Verifies that test *.cpp files contain exactly one TEST(...) macro,
 *        have a required header Doxygen block at line 1 (with @file <filename> and copyright),
 *        and that each function or TEST is immediately preceded by a Doxygen-style block comment.
 */

#include <string>
#include <vector>
#include <cctype>
#include <cstddef>
#include <utility>

using namespace clang;

namespace {
#if __has_include(<clang/AST/AST.h>)
static_assert(crsce::otpf::kClangIncludesMarker, "");
using StrRef = llvm::StringRef;
inline StrRef make_ref(const std::string &s) { return StrRef(s.data(), s.size()); }
#else
using StrRef = std::string_view;
inline StrRef make_ref(const std::string &s) { return std::string_view{s}; }
#endif

inline bool ends_with_ref(StrRef s, const char* suf) {
#if __has_include(<clang/AST/AST.h>)
  return s.ends_with(suf);
#else
  const auto n = std::char_traits<char>::length(suf);
  return s.size() >= n && s.substr(s.size() - n) == StrRef(suf, n);
#endif
}
inline StrRef rtrim_ref(StrRef s) {
#if __has_include(<clang/AST/AST.h>)
  return s.rtrim();
#else
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
    s.remove_suffix(1);
  }
  return s;
#endif
}
inline bool contains_ref(StrRef s, const char* needle) {
#if __has_include(<clang/AST/AST.h>)
  return s.contains(needle);
#else
  return s.find(needle) != StrRef::npos;
#endif
}

inline bool isBlankLine(StrRef L) {
  for (const char c : L) {
    if (c == '\n' || c == '\r') {
      break;
    }
    if (!std::isspace(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

class FileState {
public:
  FileState() = default;
  void setPath(std::string p) { Path_ = std::move(p); }
  [[nodiscard]] const std::string &path() const { return Path_; }
  void setFid(FileID id) { FID_ = id; }
  [[nodiscard]] FileID fid() const { return FID_; }
  void setContent(std::string c) { Content_ = std::move(c); }
  [[nodiscard]] const std::string &content() const { return Content_; }
  void buildLineIndex() {
    LineOffsets_.clear();
    LineOffsets_.push_back(0);
    const auto e = static_cast<unsigned>(Content_.size());
    for (unsigned i = 0; i < e; ++i) {
      if (Content_[i] == '\n') {
        LineOffsets_.push_back(i + 1);
      }
    }
  }
  [[nodiscard]] StrRef getLine(unsigned OneBased) const {
    if (OneBased == 0) {
      return {};
    }
    if (LineOffsets_.empty()) {
      return {};
    }
    if (OneBased > LineOffsets_.size()) {
      const unsigned start = LineOffsets_.back();
      if (start >= Content_.size()) {
        return {};
      }
      const StrRef whole = make_ref(Content_);
      return whole.substr(start, whole.size() - start);
    }
    const unsigned start = LineOffsets_[OneBased - 1];
    const unsigned end = (OneBased < LineOffsets_.size()) ? LineOffsets_[OneBased] : Content_.size();
    if (end < start) {
      return {};
    }
    const StrRef whole = make_ref(Content_);
    return whole.substr(start, end - start);
  }
  [[nodiscard]] bool hasImmediateDoxygenBlockAbove(unsigned targetLine, bool requireBrief) const {
    if (targetLine <= 1) {
      return false;
    }
    int L = static_cast<int>(targetLine) - 1;
    while (L >= 1 && isBlankLine(getLine(static_cast<unsigned>(L)))) { --L; }
    if (L < 1) {
      return false;
    }
    const StrRef line = rtrim_ref(getLine(static_cast<unsigned>(L)));
    if (!ends_with_ref(line, "*/")) {
      return false;
    }
    bool sawOpening = false;
    bool sawBrief = false;
    for (int i = L; i >= 1; --i) {
      const StrRef cur = getLine(static_cast<unsigned>(i));
      if (contains_ref(cur, "/**")) { sawOpening = true; break; }
      if (contains_ref(cur, "/*")) {
        return false;
      }
      if (requireBrief && contains_ref(cur, "@brief")) {
        sawBrief = true;
      }
    }
    if (!sawOpening) {
      return false;
    }
    if (requireBrief && !sawBrief) {
      return false;
    }
    return true;
  }
  void incrementTestCount() { ++TestCount_; }
  [[nodiscard]] unsigned testCount() const { return TestCount_; }
  [[nodiscard]] SourceLocation firstTestLoc() const { return FirstTestLoc_; }
  void setFirstTestLocIfUnset(SourceLocation loc) {
    if (!FirstTestLoc_.isValid()) {
      FirstTestLoc_ = loc;
    }
  }
  [[nodiscard]] bool headerChecked() const { return HeaderChecked_; }
  void setHeaderChecked(bool v) { HeaderChecked_ = v; }
  [[nodiscard]] bool headerOk() const { return HeaderOk_; }
  void setHeaderOk(bool v) { HeaderOk_ = v; }
private:
  std::string Path_;
  FileID FID_{};
  std::string Content_;
  std::vector<unsigned> LineOffsets_;
  unsigned TestCount_{0};
  SourceLocation FirstTestLoc_;
  bool HeaderChecked_{false};
  bool HeaderOk_{false};
};

#if __has_include(<clang/AST/AST.h>)
class OneTestPerFileContext {
public:
  explicit OneTestPerFileContext(CompilerInstance &CI) : CI_(&CI) {}

  FileState &getOrCreateFileState(SourceLocation Loc) {
    const SourceManager &SM = CI_->getSourceManager();
    const SourceLocation Spelling = SM.getSpellingLoc(Loc);
    const FileID FID = SM.getFileID(Spelling);
    auto It = States.find(FID.getHashValue());
    if (It != States.end()) {
      return It->second;
    }

    FileState FS;
    FS.setFid(FID);
    const llvm::StringRef Path = SM.getFilename(Spelling);
    FS.setPath(std::string(Path));
    bool Invalid = false;
    const llvm::StringRef Data = SM.getBufferData(FID, &Invalid);
    if (!Invalid) {
      FS.setContent(std::string(Data));
      FS.buildLineIndex();
    }
    auto Res = States.emplace(FID.getHashValue(), std::move(FS));
    return Res.first->second;
  }

  [[nodiscard]] bool isTestCppPath(llvm::StringRef Path) const {
    if (!Path.ends_with(".cpp")) {
      return false;
    }
    // Normalize separators and case-sensitively match '/test/'
    return Path.contains("/test/") || Path.contains("\\test\\");
  }

  void ensureHeaderChecked(FileState &FS) {
    if (FS.headerChecked()) {
      return;
    }
    FS.setHeaderChecked(true);
    if (!isTestCppPath(FS.path())) {
      return;
    }
    const SourceManager &SM = CI_->getSourceManager();
    SourceLocation StartLoc = SM.getLocForStartOfFile(FS.fid());

    auto reportHeaderError = [&](llvm::StringRef Msg) {
      const auto DiagID = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "test file must start with the required docstring header");
      CI_->getDiagnostics().Report(StartLoc, DiagID);
      const auto NoteID = CI_->getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Note, "%0");
      CI_->getDiagnostics().Report(StartLoc, NoteID) << Msg;
    };

    const llvm::StringRef C(FS.content());
    // Must start exactly at offset 0 with '/**'
    if (!C.starts_with("/**")) {
      FS.setHeaderOk(false);
      reportHeaderError("missing '/**' Doxygen block at line 1");
      return;
    }

    // Find end of first block '*/'
    const std::size_t endPos = C.find("*/");
    if (endPos == llvm::StringRef::npos) {
      FS.setHeaderOk(false);
      reportHeaderError("unterminated Doxygen block comment at file start");
      return;
    }
    const llvm::StringRef headerBlock = C.take_front(endPos + 2);

    // Check for @file <basename>
    llvm::SmallString<256> base;
    base = llvm::sys::path::filename(llvm::StringRef(FS.path()));
    llvm::SmallString<256> fileTag;
    fileTag.append("@file ");
    fileTag.append(base);
    const bool hasFile = headerBlock.contains(fileTag);

    // Check copyright details (project-specific)
    const bool hasCopyright = headerBlock.contains("@copyright") &&
                        headerBlock.contains("Sam Caldwell") &&
                        headerBlock.contains("LICENSE.txt");

    FS.setHeaderOk(hasFile && hasCopyright);
    if (!FS.headerOk()) {
      if (!hasFile) {
        reportHeaderError("missing '@file <filename>' matching current file name");
      }
      if (!hasCopyright) {
        reportHeaderError("missing copyright line (expected to reference Sam Caldwell and LICENSE.txt)");
      }
    }
  }

  void checkTestCountAndReport(FileState &FS) {
    if (!isTestCppPath(FS.path())) {
      return;
    }
    if (FS.testCount() <= 1) {
      return;
    }
    const auto DiagID = CI_->getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "only one TEST(...) is allowed per test file; found %0");
    auto DB = CI_->getDiagnostics().Report(FS.firstTestLoc().isValid() ? FS.firstTestLoc() : SourceLocation(), DiagID);
    DB << FS.testCount();
  }

  CompilerInstance &getCI() { return *CI_; }

private:
  CompilerInstance *CI_{};
  std::unordered_map<unsigned, FileState> States; // keyed by FileID hash
};

class OTPFPPCallbacks : public PPCallbacks {
public:
  explicit OTPFPPCallbacks(OneTestPerFileContext *Ctx)
      : Ctx_(Ctx) {}

  void MacroExpands(const Token &MacroNameTok, const MacroDefinition &/*MD*/,
                    SourceRange /*Range*/, const MacroArgs */*Args*/) override {
    const IdentifierInfo *II = MacroNameTok.getIdentifierInfo();
    if (!II) {
      return;
    }
    const llvm::StringRef Name = II->getName();
    if (Name != "TEST") {
      return; // Only enforce for TEST(...)
    }

    const SourceLocation Loc = MacroNameTok.getLocation();
    const SourceManager &SM = Ctx_->getCI().getSourceManager();
    const SourceLocation ExpLoc = SM.getExpansionLoc(Loc);
    FileState &FS = Ctx_->getOrCreateFileState(ExpLoc);

    if (!Ctx_->isTestCppPath(FS.path())) {
      return;
    }

    Ctx_->ensureHeaderChecked(FS);

    FS.incrementTestCount();
    FS.setFirstTestLocIfUnset(ExpLoc);

    // Verify that a Doxygen block comment immediately precedes the TEST(...) line
    const unsigned line = SM.getSpellingLineNumber(ExpLoc);
    const bool ok = FS.hasImmediateDoxygenBlockAbove(line, /*requireBrief=*/true);
    if (!ok) {
      const unsigned DiagID = Ctx_->getCI().getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "TEST(...) must be immediately preceded by a Doxygen block (/** ... */) with @brief");
      Ctx_->getCI().getDiagnostics().Report(ExpLoc, DiagID);
    }
  }

  void EndOfMainFile() override {
    // At end of main file, ensure the current main file's test count is valid.
    const SourceManager &SM = Ctx_->getCI().getSourceManager();
    const FileID MainFID = SM.getMainFileID();
    const SourceLocation MainLoc = SM.getLocForStartOfFile(MainFID);
    FileState &FS = Ctx_->getOrCreateFileState(MainLoc);
    if (!Ctx_->isTestCppPath(FS.path())) {
      return;
    }
    Ctx_->ensureHeaderChecked(FS);
    Ctx_->checkTestCountAndReport(FS);
  }

private:
  OneTestPerFileContext *Ctx_;
};

class OTPFASTVisitor : public RecursiveASTVisitor<OTPFASTVisitor> {
public:
  explicit OTPFASTVisitor(OneTestPerFileContext *Ctx) : Ctx_(Ctx) {}

  bool VisitFunctionDecl(const FunctionDecl *FD) {
    if (!FD) {
      return true;
    }
    if (!FD->isThisDeclarationADefinition()) {
      return true;
    }
    if (FD->isImplicit()) {
      return true;
    }

    const SourceManager &SM = Ctx_->getCI().getSourceManager();
    const SourceLocation Loc = FD->getBeginLoc();
    const SourceLocation Spelling = SM.getSpellingLoc(Loc);
    FileState &FS = Ctx_->getOrCreateFileState(Spelling);

    if (!Ctx_->isTestCppPath(FS.path())) {
      return true;
    }

    Ctx_->ensureHeaderChecked(FS);

    const unsigned line = SM.getSpellingLineNumber(Spelling);
    const bool ok = FS.hasImmediateDoxygenBlockAbove(line, /*requireBrief=*/true);
    if (!ok) {
      const unsigned DiagID = Ctx_->getCI().getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "function definition must be immediately preceded by a Doxygen block (/** ... */) with @brief");
      Ctx_->getCI().getDiagnostics().Report(Spelling, DiagID);
    }

    return true;
  }

private:
  OneTestPerFileContext *Ctx_;
};

class OTPFASTConsumer : public ASTConsumer {
public:
  explicit OTPFASTConsumer(OneTestPerFileContext *Ctx) : Visitor(Ctx) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OTPFASTVisitor Visitor;
};

class OneTestPerFileAction : public PluginASTAction {
public:
  OneTestPerFileAction() = default;
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef /*InFile*/) override {
    Ctx = std::make_unique<OneTestPerFileContext>(CI);
    // Register PP callbacks
    CI.getPreprocessor().addPPCallbacks(std::make_unique<OTPFPPCallbacks>(Ctx.get()));
    return std::make_unique<OTPFASTConsumer>(Ctx.get());
  }

  bool ParseArgs(const CompilerInstance &CI, const std::vector<std::string> &args) override {
    // No args currently
    (void)CI; (void)args;
    return true;
  }

  PluginASTAction::ActionType getActionType() override {
    return AddBeforeMainAction;
  }

private:
  std::unique_ptr<OneTestPerFileContext> Ctx;
};

namespace {
const FrontendPluginRegistry::Add<OneTestPerFileAction>
    X("one-test-per-file", "enforce one TEST per file and docstrings in tests");
} // anonymous namespace
#endif // has clang headers

} // namespace
