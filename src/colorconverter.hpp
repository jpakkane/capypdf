// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#pragma once

#include <pdfcommon.hpp>

// To avoid pulling all of LittleCMS in this file.
typedef void *cmsHPROFILE;

namespace capypdf::internal {

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
    static rvoe<PdfColorConverter> construct(const pystd2025::Path &rgb_profile_fname,
                                             const pystd2025::Path &gray_profile_fname,
                                             const pystd2025::Path &cmyk_profile_fname);

    PdfColorConverter(PdfColorConverter &&o) = default;
    ~PdfColorConverter();

    rvoe<DeviceRGBColor> to_rgb(const DeviceCMYKColor &cmyk);

    DeviceGrayColor to_gray(const DeviceRGBColor &rgb);
    rvoe<DeviceGrayColor> to_gray(const DeviceCMYKColor &cmyk);
    rvoe<DeviceCMYKColor> to_cmyk(const DeviceRGBColor &rgb);

    rvoe<RawPixelImage> convert_image_to(RawPixelImage ri,
                                         CapyPDF_Device_Colorspace output_format,
                                         CapyPDF_Rendering_Intent intent) const;

    std::span<std::byte> get_rgb();
    std::span<std::byte> get_gray();
    std::span<std::byte> get_cmyk();

    rvoe<int> get_num_channels(std::span<std::byte> icc_data) const;

    PdfColorConverter &operator=(PdfColorConverter &&o) = default;

private:
    PdfColorConverter();

    cmsHPROFILE profile_for(CapyPDF_Device_Colorspace cs) const;
    cmsHPROFILE profile_for(CapyPDF_Image_Colorspace cs) const;

    LcmsHolder rgb_profile;
    LcmsHolder gray_profile;
    LcmsHolder cmyk_profile;

    std::vector<std::byte> rgb_profile_data, gray_profile_data, cmyk_profile_data;
    // FIXME, store transforms so that they don't get recreated all the time.
};

} // namespace capypdf::internal
