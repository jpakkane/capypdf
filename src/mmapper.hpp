// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <pystd2025.hpp>

namespace capypdf::internal {

class MMapperPrivate;

class MMapper {
public:
    MMapper() noexcept = default;
    explicit MMapper(MMapperPrivate *priv);
    MMapper(MMapper &&o) noexcept;

    MMapper(const MMapper &o);

    ~MMapper();

    pystd2025::BytesView span() const;
    pystd2025::CStringView sv() const;

    MMapper &operator=(MMapper &&o) noexcept;
    MMapper &operator=(const MMapper &o) = delete;

private:
    pystd2025::unique_ptr<MMapperPrivate> d;
};

typedef pystd2025::Variant<pystd2025::Monostate, MMapper, pystd2025::Bytes, pystd2025::BytesView>
    DataSource;

rvoe<pystd2025::BytesView> span_of_source(const DataSource &s);
rvoe<pystd2025::CStringView> view_of_source(const DataSource &s);

rvoe<MMapper> mmap_file(const char *fname);

} // namespace capypdf::internal
