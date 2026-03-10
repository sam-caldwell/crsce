/**
 * @file crsce_viewer/viewer.h
 * @copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.
 * @brief CRSCE container viewer: parse and print header + block data.
 */
#pragma once

#include <iosfwd>
#include <string>

namespace crsce::viewer {

    /**
     * @name run_viewer
     * @brief Parse a .crsce file and write human-readable output.
     * @param path Filesystem path to the .crsce container.
     * @param out Output stream for the formatted report.
     * @param err Output stream for error messages.
     * @return 0 on success, 1 on error.
     */
    int run_viewer(const std::string &path, std::ostream &out, std::ostream &err);

} // namespace crsce::viewer
