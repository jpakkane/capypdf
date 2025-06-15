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

struct cmsdata {
    LcmsHolder rgb_profile;
    LcmsHolder gray_profile;
    LcmsHolder cmyk_profile;

    pystd2025::Bytes rgb_profile_data, gray_profile_data, cmyk_profile_data;
};

class PdfColorConverter {
public:
    PdfColorConverter() noexcept;

    PdfColorConverter(PdfColorConverter &&o) noexcept = default;
    PdfColorConverter(cmsdata d_) noexcept : d(pystd2025::move(d_)) {}
    ~PdfColorConverter();

    rvoe<DeviceRGBColor> to_rgb(const DeviceCMYKColor &cmyk);

    DeviceGrayColor to_gray(const DeviceRGBColor &rgb);
    rvoe<DeviceGrayColor> to_gray(const DeviceCMYKColor &cmyk);
    rvoe<DeviceCMYKColor> to_cmyk(const DeviceRGBColor &rgb);

    rvoe<RawPixelImage> convert_image_to(RawPixelImage ri,
                                         CapyPDF_Device_Colorspace output_format,
                                         CapyPDF_Rendering_Intent intent) const;

    pystd2025::BytesView get_rgb();
    pystd2025::BytesView get_gray();
    pystd2025::BytesView get_cmyk();

    rvoe<int> get_num_channels(pystd2025::BytesView icc_data) const;

    PdfColorConverter &operator=(PdfColorConverter &&o) noexcept = default;

private:
    cmsHPROFILE profile_for(CapyPDF_Device_Colorspace cs) const;
    cmsHPROFILE profile_for(CapyPDF_Image_Colorspace cs) const;

    cmsdata d;
    // FIXME, store transforms so that they don't get recreated all the time.
};

rvoe<PdfColorConverter> construct_colorconverter(const pystd2025::Path &rgb_profile_fname,
                                                 const pystd2025::Path &gray_profile_fname,
                                                 const pystd2025::Path &cmyk_profile_fname);

} // namespace capypdf::internal
