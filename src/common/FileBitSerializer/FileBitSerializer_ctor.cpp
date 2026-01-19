/**
 * @file FileBitSerializer_ctor.cpp
 * @copyright (c) 2026 Sam Caldwell.  See LICENSE.txt for details.
 * @brief Constructor for FileBitSerializer: open stream and reset state.
 */
#include "common/FileBitSerializer.h"
#include <ios>
#include <string>

namespace crsce::common {

/**
 * @name FileBitSerializer
 * @brief Construct a bit-serializer for the given path.
 * @usage FileBitSerializer s("/path/to/file");
 * @throws None (stream state is observable via good()).
 * @param path Filesystem path to open in binary mode.
 * @return N/A
 */
FileBitSerializer::FileBitSerializer(const std::string &path) // GCOVR_EXCL_LINE
    : in_(path, std::ios::binary), buf_(kChunkSize) {}

} // namespace crsce::common
