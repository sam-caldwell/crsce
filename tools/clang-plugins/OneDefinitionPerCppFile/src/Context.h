/**
 * @file Context.h
 * @brief Declarations for plugin context, options, and construct tracking.
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt.
 */
#pragma once

#include <string>
#include <vector>

#include <clang/AST/Decl.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <llvm/ADT/StringRef.h>

namespace odpcpp {

/**
 * @struct ConstructInfo
 * @brief Information about a discovered construct for diagnostics.
 */
struct ConstructInfo {
  unsigned line{};           ///< One-based source line of the construct
  std::string kind;          ///< Kind label (function, ctor, method, enum, ...)
  std::string qname;         ///< Fully qualified name
  bool countable{false};     ///< Whether this construct counts toward the rule
};

/**
 * @struct Options
 * @brief Runtime options controlling enforcement and verbosity.
 */
struct Options {
  bool enforceHeader{true};    ///< Enforce TU header block tags
  bool requireBrief{true};     ///< Require @brief in doc blocks
  bool enforceDoc{true};       ///< Enforce per-definition doc
  bool printConstructs{false}; ///< Print per-construct status
  bool debugErrors{false};     ///< Escalate TU summary to Error (debug runs)
};

/**
 * @class Ctx
 * @brief Plugin context: tracks options, constructs, and performs checks.
 */
class Ctx {
public:
  /**
   * @brief Construct a plugin context bound to a compiler instance.
   * @param CI Clang compiler instance.
   */
  explicit Ctx(clang::CompilerInstance &CI);

  /**
   * @brief Set runtime options.
   * @param O Options to apply.
   */
  void setOptions(Options O);

  /**
   * @brief Add a discovered construct and run per-definition doc checks.
   * @param D AST declaration pointer (definition).
   * @param kind Human-readable kind label.
   * @param countable Whether this construct counts toward one-definition rule.
   */
  void addConstruct(const clang::Decl *D, llvm::StringRef kind, bool countable);

  /**
   * @brief Finalize TU: enforce header doc and one-definition rule; emit summary.
   */
  void finalizeTU();

  /**
   * @brief Provide ASTContext/SourceManager for the active TU prior to matching.
   */
  void setASTContext(clang::ASTContext &C);

  /**
   * @brief Access underlying compiler instance.
   * @return Reference to associated compiler instance.
   */
  clang::CompilerInstance &ci();

  // Static helpers (exposed for tests if needed)
  static bool hasDocBrief(clang::CompilerInstance &CI, const clang::Decl *D, bool requireBrief);
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

  clang::CompilerInstance *CI_{};   ///< Owning compiler instance (for diagnostics)
  clang::ASTContext *AST_{};        ///< Active ASTContext (per TU)
  clang::SourceManager *SM_{};      ///< Active SourceManager (per TU)
  Options Opt_{};                   ///< Effective runtime options
  std::vector<ConstructInfo> Constructs_; ///< Discovered constructs
  bool HadError_{false};            ///< Tracks if any error was emitted
};

} // namespace odpcpp
