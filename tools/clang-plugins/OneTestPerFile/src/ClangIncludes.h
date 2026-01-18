/**
 * @file ClangIncludes.h
 * @brief System-marked aggregation of Clang/LLVM headers to suppress third-party warnings in analysis.
 */
#pragma once
#pragma clang system_header

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceManager.h>
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Path.h"

namespace crsce::otpf {
inline constexpr bool kClangIncludesMarker = true;
}
