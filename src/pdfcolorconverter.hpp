// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

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

    DeviceRGBColor to_rgb(const DeviceCMYKColor &cmyk);

    DeviceGrayColor to_gray(const DeviceRGBColor &rgb);
    DeviceGrayColor to_gray(const DeviceCMYKColor &cmyk);
    rvoe<DeviceCMYKColor> to_cmyk(const DeviceRGBColor &rgb);

    rvoe<RasterImage> convert_image_to(RasterImage ri,
                                       const ImageLoadParameters &params,
                                       CapyPDF_Colorspace output_format) const;

    const std::string &get_rgb() const { return rgb_profile_data; }
    const std::string &get_gray() const { return gray_profile_data; }
    const std::string &get_cmyk() const { return cmyk_profile_data; }

    rvoe<int> get_num_channels(std::string_view icc_data) const;

    PdfColorConverter &operator=(PdfColorConverter &&o) = default;

private:
    PdfColorConverter();

    cmsHPROFILE profile_for(CapyPDF_Colorspace cs) const;

    LcmsHolder rgb_profile;
    LcmsHolder gray_profile;
    LcmsHolder cmyk_profile;

    std::string rgb_profile_data, gray_profile_data, cmyk_profile_data;
    // FIXME, store transforms so that they don't get recreated all the time.
};

} // namespace capypdf
