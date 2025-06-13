// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <string_view>
#include <variant>

#include <pystd2025.hpp>

namespace capypdf::internal {

class MMapperPrivate;

class MMapper {
public:
    explicit MMapper(MMapperPrivate *priv);
    MMapper(MMapper &&o);
    MMapper() = delete;

    ~MMapper();

    pystd2025::BytesView span() const;
    std::string_view sv() const;

    MMapper &operator=(MMapper &&o);
    MMapper &operator=(const MMapper &o) = delete;

private:
    pystd2025::unique_ptr<MMapperPrivate> d;
};

typedef std::variant<std::monostate, MMapper, pystd2025::Bytes, pystd2025::BytesView> DataSource;

rvoe<pystd2025::BytesView> span_of_source(const DataSource &s);
rvoe<std::string_view> view_of_source(const DataSource &s);

rvoe<MMapper> mmap_file(const char *fname);

} // namespace capypdf::internal
