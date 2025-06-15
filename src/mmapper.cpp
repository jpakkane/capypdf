// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <mmapper.hpp>
#include <utils.hpp>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace capypdf::internal {

#ifdef _WIN32

class MMapperPrivate {
public:
    MMapperPrivate(HANDLE fh, HANDLE mh, const void *buf, uint64_t bufsize)
        : file_handle{fh}, mapping_handle{mh}, buf{buf}, bufsize{bufsize} {}
    ~MMapperPrivate() {
        UnmapViewOfFile(buf);
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
    }

    pystd2025::BytesView span() const { return pystd2025::BytesView((const char *)buf, bufsize); }

    pystd2025::CStringView sv() const { return pystd2025::CStringView((const char *)buf, bufsize); }

private:
    HANDLE file_handle;
    HANDLE mapping_handle;
    const void *buf;
    uint64_t bufsize;
};

#else

class MMapperPrivate {
public:
    MMapperPrivate(const char *buf, size_t bufsize) : buf{buf}, bufsize{bufsize} {};
    ~MMapperPrivate() {
        if(munmap((void *)buf, bufsize) != 0) {
            perror(nullptr);
            return;
        }
    }

    pystd2025::BytesView span() const { return pystd2025::BytesView(buf, bufsize); }

    pystd2025::CStringView sv() const { return pystd2025::CStringView(buf, bufsize); }

private:
    const char *buf;
    size_t bufsize;
};

#endif

MMapper::MMapper(MMapperPrivate *priv) : d{priv} {}

MMapper::~MMapper() = default;

MMapper::MMapper(MMapper &&o) noexcept = default;

MMapper::MMapper(const MMapper &o) { throw "Nope, can not copy these."; }

MMapper &MMapper::operator=(MMapper &&o) noexcept = default;

pystd2025::BytesView MMapper::span() const { return d->span(); }

pystd2025::CStringView MMapper::sv() const { return d->sv(); }

#ifdef _WIN32

rvoe<MMapper> mmap_file(const char *fname) {
    HANDLE file_handle = CreateFile(fname,
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if(file_handle == INVALID_HANDLE_VALUE) {
        RETERR(CouldNotOpenFile);
    }

    DWORD size_low, size_high;
    size_low = GetFileSize(file_handle, &size_high);
    if(size_low == INVALID_FILE_SIZE) {
        CloseHandle(file_handle);
        RETERR(CouldNotOpenFile);
    }

    const uint64_t bufsize = (((uint64_t)size_high) << 32) + size_low;
    HANDLE mapping_handle = CreateFileMapping(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if(!mapping_handle) {
        CloseHandle(file_handle);
        RETERR(MMapFail);
    }
    auto *buf = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
    if(!buf) {
        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        RETERR(MMapFail);
    }

    return MMapper(new MMapperPrivate(file_handle, mapping_handle, buf, bufsize));
}

#else

rvoe<MMapper> mmap_file(const char *fname) {
    FILE *f = fopen(fname, "r");
    if(!f) {
        RETERR(CouldNotOpenFile);
    }
    fseek(f, 0, SEEK_END);
    const auto bufsize = ftell(f);
    if(bufsize == 0) {
        fclose(f);
        RETERR(FileReadError);
    }
    fseek(f, 0, SEEK_SET);
    int fd = fileno(f);
    auto *buf = mmap(nullptr, bufsize, PROT_READ, MAP_PRIVATE, fd, 0);
    fclose(f);
    if(buf == MAP_FAILED) {
        RETERR(MMapFail);
    }
    return MMapper(new MMapperPrivate((const char *)buf, bufsize));
}

#endif

rvoe<pystd2025::BytesView> span_of_source(const DataSource &s) {
    if(auto *mm = s.get_if<MMapper>()) {
        return mm->span();
    }
    if(auto *by = s.get_if<pystd2025::Bytes>()) {
        return by->view();
    }
    if(auto *sp = s.get_if<pystd2025::BytesView>()) {
        return *sp;
    }
    fprintf(stderr, "Tried to use an empty datasource for font data.\n");
    abort();
}

rvoe<pystd2025::CStringView> view_of_source(const DataSource &s) {
    if(auto *mm = s.get_if<MMapper>()) {
        return mm->sv();
    }
    if(auto *sv = s.get_if<pystd2025::BytesView>()) {
        return span2sv(*sv);
    }
    fprintf(stderr, "Tried to use an empty datasource for font data.\n");
    abort();
}

} // namespace capypdf::internal
