/**
 * @file ClangIncludes.h
 * @brief System-marked aggregation of Clang/LLVM headers for tidy.
 */
#pragma once
#pragma clang system_header

#if __has_include(<llvm/ADT/StringRef.h>)
#include <llvm/ADT/StringRef.h>
#endif
#if __has_include(<llvm/Support/Path.h>)
#include <llvm/Support/Path.h>
#endif
#if __has_include(<clang/AST/AST.h>)
#include <clang/AST/AST.h>
#endif
#if __has_include(<clang/AST/ASTConsumer.h>)
#include <clang/AST/ASTConsumer.h>
#endif
#if __has_include(<clang/AST/ASTContext.h>)
#include <clang/AST/ASTContext.h>
#endif
#if __has_include(<clang/AST/Decl.h>)
#include <clang/AST/Decl.h>
#endif
#if __has_include(<clang/AST/RecursiveASTVisitor.h>)
#include <clang/AST/RecursiveASTVisitor.h>
#endif
#if __has_include(<clang/Basic/Diagnostic.h>)
#include <clang/Basic/Diagnostic.h>
#endif
#if __has_include(<clang/Basic/SourceManager.h>)
#include <clang/Basic/SourceManager.h>
#endif
#if __has_include(<clang/Frontend/CompilerInstance.h>)
#include <clang/Frontend/CompilerInstance.h>
#endif
#if __has_include(<clang/Frontend/FrontendAction.h>)
#include <clang/Frontend/FrontendAction.h>
#endif
#if __has_include(<clang/Frontend/FrontendPluginRegistry.h>)
#include <clang/Frontend/FrontendPluginRegistry.h>
#endif
#if __has_include(<clang/Lex/Preprocessor.h>)
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#endif

namespace odph { inline constexpr bool kClangIncludes = true; }
