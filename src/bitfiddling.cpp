#include <bitfiddling.hpp>

import std;

namespace capypdf::internal {

rvoe<std::span<const std::byte>> get_substring(const char *buf,
                                               const int64_t bufsize,
                                               const int64_t offset,
                                               const int64_t substr_size) {
    if(!buf) {
        RETERR(ArgIsNull);
    }
    if(bufsize < 0 || offset < 0 || substr_size < 0) {
        RETERR(IndexIsNegative);
    }
    if(offset > bufsize) {
        RETERR(IndexOutOfBounds);
    }
    if(offset + substr_size > bufsize) {
        RETERR(IndexOutOfBounds);
    }
    if(substr_size == 0) {
        return std::span<const std::byte>{};
    }
    return std::span<const std::byte>((const std::byte *)buf + offset, substr_size);
}

rvoe<std::span<const std::byte>>
get_substring(std::span<const std::byte> sv, const size_t offset, const int64_t substr_size) {
    return get_substring((const char *)sv.data(), sv.size(), offset, substr_size);
}

} // namespace capypdf::internal
