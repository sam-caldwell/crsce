/**
 * @file OneTestPerFile.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 */
/*
 * Brief: Verifies that test *.cpp files contain exactly one TEST(...) macro,
 *        have a required header Doxygen block at line 1 (with @file <filename> and copyright),
 *        and that each function or TEST is immediately preceded by a Doxygen-style block comment.
 */

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>

using namespace clang;

namespace {

static inline bool isBlankLine(llvm::StringRef L) {
  for (char c : L) {
    if (c == '\n' || c == '\r') {
      break;
    }
    if (!std::isspace(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

struct FileState {
  std::string Path;
  FileID FID;
  std::string Content;
  std::vector<unsigned> LineOffsets; // 1-based: Line N starts at offset LineOffsets[N-1]
  unsigned TestCount = 0;
  SourceLocation FirstTestLoc;
  bool HeaderChecked = false;
  bool HeaderOk = false;

  void buildLineIndex() {
    LineOffsets.clear();
    LineOffsets.push_back(0); // line 1 starts at 0
    for (unsigned i = 0, e = Content.size(); i < e; ++i) {
      if (Content[i] == '\n') {
        LineOffsets.push_back(i + 1);
      }
    }
  }

  llvm::StringRef getLine(unsigned OneBased) const {
    if (OneBased == 0) {
      return {};
    }
    if (LineOffsets.empty()) {
      return {};
    }
    if (OneBased > LineOffsets.size()) {
      // last line may not end with newline
      unsigned start = LineOffsets.back();
      if (start >= Content.size()) {
        return {};
      }
      return llvm::StringRef(Content.data() + start, Content.size() - start);
    }
    unsigned start = LineOffsets[OneBased - 1];
    unsigned end = (OneBased < LineOffsets.size()) ? LineOffsets[OneBased] : Content.size();
    if (end < start) {
      return {};
    }
    return llvm::StringRef(Content.data() + start, end - start);
  }

  // Check if there is a Doxygen-style block comment (/** ... */) that ends
  // immediately above the target line, allowing only blank lines in between.
  bool hasImmediateDoxygenBlockAbove(unsigned targetLine, bool requireBrief) const {
    if (targetLine <= 1) {
      return false;
    }
    int L = static_cast<int>(targetLine) - 1; // line directly above
    // Skip blank lines
    while (L >= 1 && isBlankLine(getLine(static_cast<unsigned>(L)))) {
      --L;
    }
    if (L < 1) {
      return false;
    }

    // The non-blank line should end with */ (closing a block comment)
    llvm::StringRef line = getLine(static_cast<unsigned>(L)).rtrim();
    if (!line.ends_with("*/")) {
      return false;
    }

    // Scan upwards to find the opening '/**'
    bool sawOpening = false;
    bool sawBrief = false;
    for (int i = L; i >= 1; --i) {
      llvm::StringRef cur = getLine(static_cast<unsigned>(i));
      if (cur.contains("/**")) {
        sawOpening = true;
        break;
      }
      if (cur.contains("/*")) {
        // Found a non-Doxygen block start; reject
        return false;
      }
      if (requireBrief && cur.contains("@brief")) {
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
};

class OneTestPerFileContext {
public:
  OneTestPerFileContext(CompilerInstance &CI) : CI(CI) {}

  FileState &getOrCreateFileState(SourceLocation Loc) {
    auto &SM = CI.getSourceManager();
    SourceLocation Spelling = SM.getSpellingLoc(Loc);
    FileID FID = SM.getFileID(Spelling);
    auto It = States.find(FID.getHashValue());
    if (It != States.end()) {
      return It->second;
    }

    FileState FS;
    FS.FID = FID;
    llvm::StringRef Path = SM.getFilename(Spelling);
    FS.Path = std::string(Path);
    bool Invalid = false;
    llvm::StringRef Data = SM.getBufferData(FID, &Invalid);
    if (!Invalid) {
      FS.Content = std::string(Data);
      FS.buildLineIndex();
    }
    auto Res = States.emplace(FID.getHashValue(), std::move(FS));
    return Res.first->second;
  }

  bool isTestCppPath(llvm::StringRef Path) const {
    if (!Path.ends_with(".cpp")) {
      return false;
    }
    // Normalize separators and case-sensitively match '/test/'
    return Path.contains("/test/") || Path.contains("\\test\\");
  }

  void ensureHeaderChecked(FileState &FS) {
    if (FS.HeaderChecked) {
      return;
    }
    FS.HeaderChecked = true;
    if (!isTestCppPath(FS.Path)) {
      return;
    }
    SourceManager &SM = CI.getSourceManager();
    SourceLocation StartLoc = SM.getLocForStartOfFile(FS.FID);

    auto reportHeaderError = [&](llvm::StringRef Msg) {
      unsigned DiagID = CI.getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "test file must start with the required docstring header");
      CI.getDiagnostics().Report(StartLoc, DiagID);
      unsigned NoteID = CI.getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Note, "%0");
      CI.getDiagnostics().Report(StartLoc, NoteID) << Msg;
    };

    llvm::StringRef C(FS.Content);
    // Must start exactly at offset 0 with '/**'
    if (!C.starts_with("/**")) {
      FS.HeaderOk = false;
      reportHeaderError("missing '/**' Doxygen block at line 1");
      return;
    }

    // Find end of first block '*/'
    size_t endPos = C.find("*/");
    if (endPos == llvm::StringRef::npos) {
      FS.HeaderOk = false;
      reportHeaderError("unterminated Doxygen block comment at file start");
      return;
    }
    llvm::StringRef headerBlock = C.take_front(endPos + 2);

    // Check for @file <basename>
    llvm::SmallString<256> base;
    base = llvm::sys::path::filename(llvm::StringRef(FS.Path));
    llvm::SmallString<256> fileTag;
    fileTag.append("@file ");
    fileTag.append(base);
    bool hasFile = headerBlock.contains(fileTag);

    // Check copyright details (project-specific)
    bool hasCopyright = headerBlock.contains("@copyright") &&
                        headerBlock.contains("Sam Caldwell") &&
                        headerBlock.contains("LICENSE.txt");

    FS.HeaderOk = hasFile && hasCopyright;
    if (!FS.HeaderOk) {
      if (!hasFile) {
        reportHeaderError("missing '@file <filename>' matching current file name");
      }
      if (!hasCopyright) {
        reportHeaderError("missing copyright line (expected to reference Sam Caldwell and LICENSE.txt)");
      }
    }
  }

  void checkTestCountAndReport(FileState &FS) {
    if (!isTestCppPath(FS.Path)) {
      return;
    }
    if (FS.TestCount <= 1) {
      return;
    }
    unsigned DiagID = CI.getDiagnostics().getCustomDiagID(
        DiagnosticsEngine::Error,
        "only one TEST(...) is allowed per test file; found %0");
    auto DB = CI.getDiagnostics().Report(FS.FirstTestLoc.isValid() ? FS.FirstTestLoc : SourceLocation(), DiagID);
    DB << FS.TestCount;
  }

  CompilerInstance &getCI() { return CI; }

private:
  CompilerInstance &CI;
  std::unordered_map<unsigned, FileState> States; // keyed by FileID hash
};

class OTPFPPCallbacks : public PPCallbacks {
public:
  explicit OTPFPPCallbacks(OneTestPerFileContext &Ctx)
      : Ctx(Ctx) {}

  void MacroExpands(const Token &MacroNameTok, const MacroDefinition &MD,
                    SourceRange Range, const MacroArgs *Args) override {
    IdentifierInfo *II = MacroNameTok.getIdentifierInfo();
    if (!II) {
      return;
    }
    llvm::StringRef Name = II->getName();
    if (Name != "TEST") {
      return; // Only enforce for TEST(...)
    }

    SourceLocation Loc = MacroNameTok.getLocation();
    SourceManager &SM = Ctx.getCI().getSourceManager();
    SourceLocation ExpLoc = SM.getExpansionLoc(Loc);
    FileState &FS = Ctx.getOrCreateFileState(ExpLoc);

    if (!Ctx.isTestCppPath(FS.Path)) {
      return;
    }

    Ctx.ensureHeaderChecked(FS);

    FS.TestCount += 1;
    if (!FS.FirstTestLoc.isValid()) {
      FS.FirstTestLoc = ExpLoc;
    }

    // Verify that a Doxygen block comment immediately precedes the TEST(...) line
    unsigned line = SM.getSpellingLineNumber(ExpLoc);
    bool ok = FS.hasImmediateDoxygenBlockAbove(line, /*requireBrief=*/true);
    if (!ok) {
      unsigned DiagID = Ctx.getCI().getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "TEST(...) must be immediately preceded by a Doxygen block (/** ... */) with @brief");
      Ctx.getCI().getDiagnostics().Report(ExpLoc, DiagID);
    }
  }

  void EndOfMainFile() override {
    // At end of main file, ensure the current main file's test count is valid.
    SourceManager &SM = Ctx.getCI().getSourceManager();
    FileID MainFID = SM.getMainFileID();
    SourceLocation MainLoc = SM.getLocForStartOfFile(MainFID);
    FileState &FS = Ctx.getOrCreateFileState(MainLoc);
    if (!Ctx.isTestCppPath(FS.Path)) {
      return;
    }
    Ctx.ensureHeaderChecked(FS);
    Ctx.checkTestCountAndReport(FS);
  }

private:
  OneTestPerFileContext &Ctx;
};

class OTPFASTVisitor : public RecursiveASTVisitor<OTPFASTVisitor> {
public:
  explicit OTPFASTVisitor(OneTestPerFileContext &Ctx) : Ctx(Ctx) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD) {
      return true;
    }
    if (!FD->isThisDeclarationADefinition()) {
      return true;
    }
    if (FD->isImplicit()) {
      return true;
    }

    SourceManager &SM = Ctx.getCI().getSourceManager();
    SourceLocation Loc = FD->getBeginLoc();
    SourceLocation Spelling = SM.getSpellingLoc(Loc);
    FileState &FS = Ctx.getOrCreateFileState(Spelling);

    if (!Ctx.isTestCppPath(FS.Path)) {
      return true;
    }

    Ctx.ensureHeaderChecked(FS);

    unsigned line = SM.getSpellingLineNumber(Spelling);
    bool ok = FS.hasImmediateDoxygenBlockAbove(line, /*requireBrief=*/true);
    if (!ok) {
      unsigned DiagID = Ctx.getCI().getDiagnostics().getCustomDiagID(
          DiagnosticsEngine::Error,
          "function definition must be immediately preceded by a Doxygen block (/** ... */) with @brief");
      Ctx.getCI().getDiagnostics().Report(Spelling, DiagID);
    }

    return true;
  }

private:
  OneTestPerFileContext &Ctx;
};

class OTPFASTConsumer : public ASTConsumer {
public:
  explicit OTPFASTConsumer(OneTestPerFileContext &Ctx) : Visitor(Ctx) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  OTPFASTVisitor Visitor;
};

class OneTestPerFileAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
    Ctx = std::make_unique<OneTestPerFileContext>(CI);
    // Register PP callbacks
    CI.getPreprocessor().addPPCallbacks(std::make_unique<OTPFPPCallbacks>(*Ctx));
    return std::make_unique<OTPFASTConsumer>(*Ctx);
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

} // namespace

static FrontendPluginRegistry::Add<OneTestPerFileAction>
    X("one-test-per-file", "enforce one TEST per file and docstrings in tests");
