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

#include <fontsubsetter.hpp>
#include <algorithm>

FontSubsetter::FontSubsetter(const char *fname) { subsets.emplace_back(std::vector<uint32_t>{}); }

FontSubsetInfo FontSubsetter::get_glyph_subset(uint32_t glyph) {
    auto trial = find_glyph(glyph);
    if(trial) {
        return trial.value();
    }
    if(subsets.back().size() == max_glyphs) {
        subsets.emplace_back(std::vector<uint32_t>{});
    }
    subsets.back().push_back(glyph);
    return FontSubsetInfo{int32_t(subsets.size() - 1), int32_t(subsets.back().size() - 1)};
}

std::optional<FontSubsetInfo> FontSubsetter::find_glyph(uint32_t glyph) const {
    for(size_t subset = 0; subset < subsets.size(); ++subset) {
        auto loc = std::find(subsets.at(subset).cbegin(), subsets.at(subset).cend(), glyph);
        if(loc != subsets[subset].cend()) {
            return FontSubsetInfo{int32_t(subset), int32_t(loc - subsets.at(subset).cbegin())};
        }
    }
    return {};
}
