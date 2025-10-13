#pragma once

#include <errorhandling.hpp>
#include <span>
#include <vector>
#include <string_view>
#include <bit>
#include <string>

#include <cstring>
#include <cassert>

namespace capypdf::internal {

rvoe<std::span<const std::byte>> get_substring(const char *buf,
                                               const int64_t bufsize,
                                               const int64_t offset,
                                               const int64_t substr_size);

rvoe<std::span<const std::byte>>
get_substring(std::span<const std::byte> sv, const size_t offset, const int64_t substr_size);

template<typename T>
rvoe<capypdf::internal::NoReturnValue>
safe_memcpy(T *obj, std::span<const std::byte> source, const uint64_t offset) {
    ERC(validated_area, get_substring(source, offset, sizeof(T)));
    assert(validated_area.size() == sizeof(T));
    memcpy(obj, validated_area.data(), sizeof(T));
    return capypdf::internal::NoReturnValue{};
}

template<typename T> rvoe<T> extract(std::span<const std::byte> bf, const size_t offset) {
    T obj;
    ERCV(safe_memcpy(&obj, bf, offset));
    return obj;
}

template<typename T> rvoe<T> extract_and_swap(std::span<const std::byte> bf, const size_t offset) {
    T obj;
    ERCV(safe_memcpy(&obj, bf, offset));
    return std::byteswap(obj);
}

template<typename T> void append_bytes(std::vector<std::byte> &s, const T &val) {
    if constexpr(std::is_same_v<T, std::string_view>) {
        s.insert(s.end(), val.cbegin(), val.cend());
    } else if constexpr(std::is_same_v<T, std::string>) {
        s.insert(s.end(), val.cbegin(), val.cend());
    } else if constexpr(std::is_same_v<T, std::span<std::byte>>) {
        s.insert(s.end(), val.begin(), val.end());
    } else if constexpr(std::is_same_v<T, std::span<const std::byte>>) {
        s.insert(s.end(), val.begin(), val.end());
    } else if constexpr(std::is_same_v<T, std::vector<std::byte>>) {
        s.insert(s.end(), val.cbegin(), val.cend());
    } else if constexpr(std::is_same_v<T, std::vector<const std::byte>>) {
        s.insert(s.end(), val.cbegin(), val.cend());
    } else {
        s.insert(s.end(), (std::byte *)&val, (std::byte *)&val + sizeof(val));
    }
}

template<typename T> void swap_and_append_bytes(std::vector<std::byte> &s, const T obj) {
    auto obj2 = std::byteswap(obj);
    append_bytes<T>(s, obj2);
}

} // namespace capypdf::internal
