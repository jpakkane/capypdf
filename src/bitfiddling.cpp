#include <bitfiddling.hpp>

namespace capypdf::internal {

rvoe<pystd2025::BytesView> get_substring(const char *buf,
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
        return pystd2025::BytesView{};
    }
    return pystd2025::BytesView(buf + offset, substr_size);
}

rvoe<pystd2025::BytesView>
get_substring(pystd2025::BytesView sv, const size_t offset, const int64_t substr_size) {
    return get_substring((const char *)sv.data(), sv.size(), offset, substr_size);
}

} // namespace capypdf::internal
