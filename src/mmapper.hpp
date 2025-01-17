// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <errorhandling.hpp>

#include <memory>
#include <span>
#include <string_view>

namespace capypdf::internal {

class MMapperPrivate;

class MMapper {
public:
    explicit MMapper(MMapperPrivate *priv);
    MMapper(MMapper &&o) = default;
    ~MMapper();

    std::span<std::byte> span() const;
    std::string_view sv() const;

private:
    std::unique_ptr<MMapperPrivate> d;
};

rvoe<MMapper> mmap_file(const char *fname);

} // namespace capypdf::internal
