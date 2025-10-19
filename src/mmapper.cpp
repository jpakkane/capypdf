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

    std::span<std::byte> span() const { return std::span<std::byte>((std::byte *)buf, bufsize); }

    std::string_view sv() const { return std::string_view((const char *)buf, bufsize); }

private:
    HANDLE file_handle;
    HANDLE mapping_handle;
    const void *buf;
    uint64_t bufsize;
};

#else

class MMapperPrivate {
public:
    MMapperPrivate(const char *buf_, size_t bufsize_) : buf{buf_}, bufsize{bufsize_} {};
    ~MMapperPrivate() {
        if(munmap((void *)buf, bufsize) != 0) {
            perror(nullptr);
            return;
        }
    }

    std::span<std::byte> span() const { return std::span<std::byte>((std::byte *)buf, bufsize); }

    std::string_view sv() const { return std::string_view(buf, bufsize); }

private:
    const char *buf;
    size_t bufsize;
};

#endif

MMapper::MMapper(MMapperPrivate *priv) : d{priv} {}

MMapper::~MMapper() = default;

MMapper::MMapper(MMapper &&o) = default;

MMapper &MMapper::operator=(MMapper &&o) = default;

std::span<std::byte> MMapper::span() const { return d->span(); }

std::string_view MMapper::sv() const { return d->sv(); }

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

rvoe<std::span<std::byte>> span_of_source(const DataSource &s) {
    if(auto *mm = std::get_if<MMapper>(&s)) {
        return mm->span();
    }
    if(auto *sv = std::get_if<std::vector<std::byte>>(&s)) {
        auto *tmp = const_cast<std::vector<std::byte> *>(sv);
        // FIXME, should be properly const.
        return std::span<std::byte>(*tmp);
    }
    if(auto *sp = std::get_if<std::span<std::byte>>(&s)) {
        return *sp;
    }
    fprintf(stderr, "Tried to use an empty datasource for font data.\n");
    std::abort();
}

rvoe<std::string_view> view_of_source(const DataSource &s) {
    if(auto *mm = std::get_if<MMapper>(&s)) {
        return mm->sv();
    }
    if(auto *sv = std::get_if<std::span<std::byte>>(&s)) {
        return span2sv(*sv);
    }
    fprintf(stderr, "Tried to use an empty datasource for font data.\n");
    std::abort();
}

} // namespace capypdf::internal
