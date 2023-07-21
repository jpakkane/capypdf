/*
 * Copyright 2022 Jussi Pakkanen
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

#include <pdfcommon.hpp>
#include <string_view>
#include <string>
#include <expected>
#include <filesystem>

// To avoid pulling all of LittleCMS in this file.
typedef void *cmsHPROFILE;

namespace capypdf {

struct LcmsHolder {
    cmsHPROFILE h;

    LcmsHolder() : h(nullptr) {}
    explicit LcmsHolder(cmsHPROFILE h) : h(h) {}
    explicit LcmsHolder(LcmsHolder &&o) : h(o.h) { o.h = nullptr; }
    ~LcmsHolder();
    void deallocate();

    LcmsHolder &operator=(const LcmsHolder &) = delete;
    LcmsHolder &operator=(LcmsHolder &&o) {
        if(this == &o) {
            return *this;
        }
        deallocate();
        h = o.h;
        o.h = nullptr;
        return *this;
    }
};

class PdfColorConverter {
public:
    static rvoe<PdfColorConverter> construct(const std::filesystem::path &rgb_profile_fname,
                                             const std::filesystem::path &gray_profile_fname,
                                             const std::filesystem::path &cmyk_profile_fname);

    PdfColorConverter(PdfColorConverter &&o) = default;
    ~PdfColorConverter();

    DeviceGrayColor to_gray(const DeviceRGBColor &rgb);
    rvoe<DeviceCMYKColor> to_cmyk(const DeviceRGBColor &rgb);

    std::string rgb_pixels_to_gray(std::string_view rgb_data);
    rvoe<std::string> rgb_pixels_to_cmyk(std::string_view rgb_data);

    const std::string &get_rgb() const { return rgb_profile_data; }
    const std::string &get_gray() const { return gray_profile_data; }
    const std::string &get_cmyk() const { return cmyk_profile_data; }

    rvoe<int> get_num_channels(std::string_view icc_data) const;

    PdfColorConverter &operator=(PdfColorConverter &&o) = default;

private:
    PdfColorConverter();
    LcmsHolder rgb_profile;
    LcmsHolder gray_profile;
    LcmsHolder cmyk_profile;

    std::string rgb_profile_data, gray_profile_data, cmyk_profile_data;
    // FIXME, store transforms so that they don't get recreated all the time.
};

} // namespace capypdf
