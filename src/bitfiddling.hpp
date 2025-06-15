#pragma once

#include <errorhandling.hpp>
#include <pystd2025.hpp>

#include <string.h>
#include <assert.h>

namespace capypdf::internal {

inline int8_t byteswap(int8_t v) { return v; }
inline uint8_t byteswap(uint8_t v) { return v; }
inline int16_t byteswap(int16_t v) { return __bswap_16(v); }
inline uint16_t byteswap(uint16_t v) { return __bswap_16(v); }
inline int32_t byteswap(int32_t v) { return __bswap_32(v); }
inline uint32_t byteswap(uint32_t v) { return __bswap_32(v); }
inline int64_t byteswap(int64_t v) { return __bswap_64(v); }
inline uint64_t byteswap(uint64_t v) { return __bswap_64(v); }

rvoe<pystd2025::BytesView> get_substring(const char *buf,
                                         const int64_t bufsize,
                                         const int64_t offset,
                                         const int64_t substr_size);

rvoe<pystd2025::BytesView>
get_substring(pystd2025::BytesView sv, const size_t offset, const int64_t substr_size);

template<typename T>
rvoe<capypdf::internal::NoReturnValue>
safe_memcpy(T *obj, pystd2025::BytesView source, const uint64_t offset) {
    ERC(validated_area, get_substring(source, offset, sizeof(T)));
    assert(validated_area.size() == sizeof(T));
    memcpy(obj, validated_area.data(), sizeof(T));
    return capypdf::internal::NoReturnValue{};
}

template<typename T> rvoe<T> extract(pystd2025::BytesView bf, const size_t offset) {
    T obj;
    ERCV(safe_memcpy(&obj, bf, offset));
    return obj;
}

template<typename T> rvoe<T> extract_and_swap(pystd2025::BytesView bf, const size_t offset) {
    T obj;
    ERCV(safe_memcpy(&obj, bf, offset));
    return byteswap(obj);
}

template<typename T> void append_bytes(pystd2025::Bytes &s, const T &val) {
    if constexpr(pystd2025::is_same_v<T, pystd2025::CStringView>) {
        s.append(val.cbegin(), val.cend());
    } else if constexpr(pystd2025::is_same_v<T, pystd2025::CString>) {
        s.append(val.cbegin(), val.cend());
    } else if constexpr(pystd2025::is_same_v<T, pystd2025::Bytes>) {
        s.append(val.begin(), val.end());
    } else if constexpr(pystd2025::is_same_v<T, pystd2025::BytesView>) {
        s.append(val.begin(), val.end());
    } else {
        s.append((const char *)&val, (const char *)&val + sizeof(val));
    }
}

template<typename T> void swap_and_append_bytes(pystd2025::Bytes &s, const T &obj) {
    auto obj2 = byteswap(obj);
    append_bytes<T>(s, obj2);
}

} // namespace capypdf::internal
