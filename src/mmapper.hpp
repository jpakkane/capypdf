// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace capypdf::internal {

class MMapperPrivate;

class MMapper {
public:
    explicit MMapper(MMapperPrivate *priv);
    MMapper(MMapper &&o);
    MMapper() = delete;

    ~MMapper();

    std::span<std::byte> span() const;
    std::string_view sv() const;

    MMapper &operator=(MMapper &&o);
    MMapper &operator=(const MMapper &o) = delete;

private:
    std::unique_ptr<MMapperPrivate> d;
};

typedef std::variant<std::monostate, MMapper, std::vector<std::byte>, std::span<std::byte>>
    DataSource;

rvoe<std::span<std::byte>> span_of_source(const DataSource &s);
rvoe<std::string_view> view_of_source(const DataSource &s);

rvoe<MMapper> mmap_file(const char *fname);

} // namespace capypdf::internal
