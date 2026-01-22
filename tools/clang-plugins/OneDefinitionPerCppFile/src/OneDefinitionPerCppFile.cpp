/**
 * @file OneDefinitionPerCppFile.cpp
 * @brief Enforce one construct per .cpp under src/ and required docstrings.
 */
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#if __has_include(<clang/Frontend/FrontendPluginRegistry.h>) && __has_include(<clang/AST/AST.h>) && __has_include( \
    <clang/Basic/SourceManager.h>) && __has_include(<clang/AST/RecursiveASTVisitor.h>) && __has_include( \
    <clang/AST/ASTConsumer.h>) && __has_include(<clang/AST/ASTContext.h>)
#include "ClangIncludes.h"
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <memory>
#include <unordered_map>
#else
#include <string_view>
#endif

namespace {
#if __has_include(<clang/Frontend/FrontendPluginRegistry.h>) && __has_include(<clang/AST/AST.h>) && __has_include( \
    <clang/Basic/SourceManager.h>) && __has_include(<clang/AST/RecursiveASTVisitor.h>) && __has_include( \
    <clang/AST/ASTConsumer.h>) && __has_include(<clang/AST/ASTContext.h>)
    static_assert(odpcpp::kClangIncludes, "");
    using StrRef = llvm::StringRef;
    inline StrRef make_ref(const std::string &s) {
        return StrRef(s.data(), s.size());
    }
#else
    using StrRef = std::string_view;
    // ReSharper disable once CppDFAUnreachableFunctionCall
    inline StrRef make_ref(const std::string &s) { return std::string_view{s}; }
#endif

    // ReSharper disable once CppDFAUnreachableFunctionCall
    inline bool isBlankLine(StrRef L) {
        for (const char c: L) {
            if (c == '\n' || c == '\r') {
                break;
            }
            if (std::isspace(static_cast<unsigned char>(c)) == 0) {
                return false;
            }
        }
        return true;
    }

    class FileState {
    public:
        void setPath(std::string p) { path_ = std::move(p); }

        [[nodiscard]] [[maybe_unused]] const std::string &path() const {
            return path_;
        }

        void setContent(std::string c) { content_ = std::move(c); }
        [[nodiscard]] const std::string &content() const { return content_; }

        void buildLineIndex() {
            lineOffsets_.clear();
            lineOffsets_.push_back(0);
            const auto e = static_cast<unsigned>(content_.size());
            for (unsigned i = 0; i < e; ++i) {
                if (content_[i] == '\n') {
                    lineOffsets_.push_back(i + 1);
                }
            }
        }

        // ReSharper disable once CppDFAUnreachableFunctionCall
        [[nodiscard]] StrRef getLine(const unsigned oneBased) const {
            if (oneBased == 0 || lineOffsets_.empty()) {
                return {};
            }
            if (oneBased > lineOffsets_.size()) {
                const unsigned start = lineOffsets_.back();
                if (start >= content_.size()) {
                    return {};
                }
                const StrRef v = make_ref(content_);
                return v.substr(start);
            }
            const unsigned start = lineOffsets_[oneBased - 1];
            const unsigned end = (oneBased < lineOffsets_.size())
                                     ? lineOffsets_[oneBased]
                                     : content_.size();
            if (end < start) {
                return {};
            }
            const StrRef v = make_ref(content_);
            return v.substr(start, end - start);
        }

        [[nodiscard]] bool hasImmediateDocAbove(unsigned L, bool requireBrief) const {
            if (L <= 1) {
                return false;
            }
            int i = static_cast<int>(L) - 1;
            while (i >= 1 && isBlankLine(getLine(static_cast<unsigned>(i)))) {
                --i;
            }
            if (i < 1) {
                return false;
            }
            StrRef line = getLine(static_cast<unsigned>(i));
#if __has_include(<clang/AST/AST.h>)
            line = line.rtrim();
            if (!line.ends_with("*/")) {
                return false;
            }
#else
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
                line.remove_suffix(1);
            }
            if (line.size() < 2 || line.substr(line.size() - 2) != StrRef("*/")) {
                return false;
            }
#endif
            bool sawOpen = false;
            bool sawBrief = false;
            for (; i >= 1; --i) {
                const StrRef cur = getLine(static_cast<unsigned>(i));
#if __has_include(<clang/AST/AST.h>)
                if (cur.contains("/**")) {
                    sawOpen = true;
                    break;
                }
                if (cur.contains("/*")) {
                    return false;
                }
                if (requireBrief && cur.contains("@brief")) {
                    sawBrief = true;
                }
#else
                if (cur.find("/**") != StrRef::npos) {
                    sawOpen = true;
                    break;
                }
                if (cur.find("/*") != StrRef::npos) {
                    return false;
                }
                if (requireBrief && cur.find("@brief") != StrRef::npos) {
                    sawBrief = true;
                }
#endif
            }
            if (!sawOpen) {
                return false;
            }
            if (requireBrief && !sawBrief) {
                return false;
            }
            return true;
        }

    private:
        std::string path_;
        std::string content_;
        std::vector<unsigned> lineOffsets_;
    };

#if __has_include(<clang/AST/AST.h>)
    class Ctx {
    public:
        explicit Ctx(clang::CompilerInstance &CI) : CI_(&CI) {
        }

        FileState &stateFor(clang::SourceLocation loc) {
            auto &SM = CI_->getSourceManager();
            auto FID = SM.getFileID(SM.getSpellingLoc(loc));
            auto it = files_.find(FID.getHashValue());
            if (it != files_.end()) {
                return it->second;
            }
            FileState fs;
            {
                auto floc = SM.getLocForStartOfFile(FID);
                auto name = SM.getFilename(floc);
                fs.setPath(std::string(name.data(), name.size()));
            }
            bool invalid = false;
            const StrRef data = SM.getBufferData(FID, &invalid);
            if (!invalid) {
                fs.setContent(std::string(data.data(), data.size()));
                fs.buildLineIndex();
            }
            auto [it2, _] = files_.emplace(FID.getHashValue(), std::move(fs));
            return it2->second;
        }

        void foundConstruct(clang::SourceLocation loc) {
            ++count_;
            auto &SM = CI_->getSourceManager();
            const auto L = SM.getSpellingLineNumber(loc);
            auto &fs = stateFor(loc);
            if (!fs.hasImmediateDocAbove(L, true)) {
                const unsigned id = CI_->getDiagnostics().getCustomDiagID(
                    clang::DiagnosticsEngine::Error,
                    "definition must be immediately preceded by a Doxygen block (/** ... "
                    "*/) with @brief");
                CI_->getDiagnostics().Report(loc, id);
            }
        }

        void finalizeTU() {
            auto &SM = CI_->getSourceManager();
            const auto FID = SM.getMainFileID();
            const auto P = SM.getFilename(SM.getLocForStartOfFile(FID));
            if (!(P.ends_with(".cpp") && P.contains("/src/"))) {
                return;
            }
            // Must start with '/**' and include @file/@brief and copyright markers.
            const FileState fs = stateFor(SM.getLocForStartOfFile(FID));
            const auto &C = fs.content();
            if (C.rfind("/**", 0) != 0 || C.find("*/") == std::string::npos ||
                C.find("@file ") == std::string::npos ||
                C.find("@brief") == std::string::npos ||
                C.find("Sam Caldwell") == std::string::npos ||
                C.find("LICENSE.txt") == std::string::npos) {
                const unsigned id = CI_->getDiagnostics().getCustomDiagID(
                    clang::DiagnosticsEngine::Error,
                    "source must start with Doxygen block containing @file/@brief and "
                    "copyright");
                CI_->getDiagnostics().Report(SM.getLocForStartOfFile(FID), id);
            }
            // @file value should match the basename of the file
            {
                llvm::SmallString<256> base;
                base = llvm::sys::path::filename(P);
                const std::string expect = std::string("@file ") + base.str().str();
                if (C.find(expect) == std::string::npos) {
                    const unsigned id2 = CI_->getDiagnostics().getCustomDiagID(
                        clang::DiagnosticsEngine::Error,
                        "source @file tag must match current file basename (%0)");
                    auto DB =
                            CI_->getDiagnostics().Report(SM.getLocForStartOfFile(FID), id2);
                    DB << base.str().str();
                }
            }
            if (count_ != 1U) {
                const unsigned id = CI_->getDiagnostics().getCustomDiagID(
                    clang::DiagnosticsEngine::Error,
                    "exactly one construct must be defined per source file; found %0");
                auto DB = CI_->getDiagnostics().Report(SM.getLocForStartOfFile(FID), id);
                DB << count_;
            }
        }

        clang::CompilerInstance &ci() { return *CI_; }

    private:
        clang::CompilerInstance *CI_{};
        std::unordered_map<unsigned, FileState> files_;
        unsigned count_{0};
    };

    class Visitor : public clang::RecursiveASTVisitor<Visitor> {
    public:
        explicit Visitor(Ctx *c) : C(c) {
        }

        bool VisitCXXRecordDecl(const clang::CXXRecordDecl *D) {
            if (valid(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

        bool VisitRecordDecl(const clang::RecordDecl *D) {
            if (valid(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

        bool VisitEnumDecl(const clang::EnumDecl *D) {
            if (valid(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

        bool VisitFunctionDecl(const clang::FunctionDecl *D) {
            if (D && D->isThisDeclarationADefinition() && !D->isImplicit() &&
                D->hasBody() && inMain(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

        bool VisitTypedefDecl(const clang::TypedefDecl *D) {
            if (D && !D->isImplicit() && inMain(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

        bool VisitTypeAliasDecl(const clang::TypeAliasDecl *D) {
            if (D && !D->isImplicit() && inMain(D)) {
                C->foundConstruct(D->getBeginLoc());
            }
            return true;
        }

    private:
        template<class T>
        bool inMain(const T *D) const {
            auto &SM = C->ci().getSourceManager();
            return SM.isWrittenInMainFile(SM.getSpellingLoc(D->getLocation()));
        }

        template<class T>
        bool valid(const T *D) const {
            return D && !D->isImplicit() && D->isThisDeclarationADefinition() &&
                   inMain(D);
        }

        Ctx *C;
    };

    class Consumer : public clang::ASTConsumer {
    public:
        explicit Consumer(Ctx *c) : V(c), C(c) {
        }

        void HandleTranslationUnit(clang::ASTContext &Context) override {
            V.TraverseDecl(Context.getTranslationUnitDecl());
            C->finalizeTU();
        }

    private:
        Visitor V;
        Ctx *C;
    };

    class Action : public clang::PluginASTAction {
    public:
        std::unique_ptr<clang::ASTConsumer>
        CreateASTConsumer(clang::CompilerInstance &CI,
                          llvm::StringRef /*InFile*/) override {
            Ctx_ = std::make_unique<Ctx>(CI);
            return std::make_unique<Consumer>(Ctx_.get());
        }

        bool ParseArgs(const clang::CompilerInstance & /*CI*/,
                       const std::vector<std::string> & /*args*/) override {
            return true;
        }

        ActionType getActionType() override { return AddBeforeMainAction; }

    private:
        std::unique_ptr<Ctx> Ctx_;
    };

    const clang::FrontendPluginRegistry::Add<Action>
    X("one-definition-per-cpp-file",
      "enforce one construct per source with docstrings");
#else
    namespace odpcpp_noop {
    }
#endif
} // namespace
