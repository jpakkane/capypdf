/*
 * Copyright 2023 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <vector>
#include <optional>
#include <cstdint>

static const std::size_t max_glyphs = 255;

struct FontSubsetInfo {
    int32_t subset;
    int32_t offset;
};

class FontSubsetter {
public:
    explicit FontSubsetter(const char *fname);

    FontSubsetInfo get_glyph_subset(uint32_t glyph);

    const std::vector<uint32_t> &get_subset(int32_t subset_number) const {
        return subsets.at(subset_number);
    }

private:
    std::optional<FontSubsetInfo> find_glyph(uint32_t glyph) const;

    std::vector<std::vector<uint32_t>> subsets;
};
