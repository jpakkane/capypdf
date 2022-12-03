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

// To avoid pulling all of LittleCMS in this file.
typedef void *cmsHPROFILE;

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
    PdfColorConverter(const char *rgb_profile,
                      const char *gray_profile,
                      const char *cmyk_profile_fname);
    ~PdfColorConverter();

    DeviceGrayColor to_gray(const DeviceRGBColor &rgb);
    DeviceCMYKColor to_cmyk(const DeviceRGBColor &rgb);

    std::string rgb_pixels_to_gray(std::string_view rgb_data);
    std::string rgb_pixels_to_cmyk(std::string_view rgb_data);

    const std::string &get_rgb() const { return rgb_profile_data; }
    const std::string &get_gray() const { return gray_profile_data; }
    const std::string &get_cmyk() const { return cmyk_profile_data; }

private:
    LcmsHolder rgb_profile;
    LcmsHolder gray_profile;
    LcmsHolder cmyk_profile;

    std::string rgb_profile_data, gray_profile_data, cmyk_profile_data;
    // FIXME, store transforms so that they don't get recreated all the time.
};
