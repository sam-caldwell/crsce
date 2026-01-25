/**
 * @file Context.h
 * @brief Declarations for ODPH plugin context, options, and construct tracking.
 */
#pragma once

#include <string>
#include <vector>

#include <clang/AST/Decl.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Basic/SourceManager.h>

namespace odph {

struct ConstructInfo {
  unsigned line{};
  std::string kind;
  std::string qname;
  bool countable{false};
};

struct Options {
  bool enforceHeader{true};
  bool requireBrief{true};
  bool enforceDoc{true};
  bool printConstructs{false};
  bool debugErrors{false};
};

class Ctx {
public:
  explicit Ctx(clang::CompilerInstance &CI);
  void setOptions(Options O);
  void setASTContext(clang::ASTContext &C);
  clang::CompilerInstance &ci();

  void addConstruct(const clang::Decl *D, llvm::StringRef kind, bool countable);
  void finalizeTU();

  struct DocRequirements {
    bool requireBlock{true};
    bool requireName{true};
    bool requireBrief{true};
    bool requireParams{false};
    bool requireReturn{false};
  };
  static bool checkDocForDecl(clang::CompilerInstance &CI,
                              const clang::Decl *D,
                              const DocRequirements &Req,
                              std::string &missingTags);

private:
  static std::string getQualifiedName(const clang::Decl *D);
  bool checkHeaderDoc(llvm::StringRef Path, clang::FileID FID);
  clang::SourceManager &SM();
  clang::DiagnosticsEngine &DE();

  clang::CompilerInstance *CI_{};
  clang::ASTContext *AST_{};
  clang::SourceManager *SM_{};
  Options Opt_{};
  std::vector<ConstructInfo> Constructs_;
  bool HadError_{false};
};

} // namespace odph
