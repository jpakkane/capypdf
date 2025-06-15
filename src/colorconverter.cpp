// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <colorconverter.hpp>
#include <utils.hpp>
#include <lcms2.h>
#include <cassert>

namespace {

const int ri2lcms_data[4] = {INTENT_RELATIVE_COLORIMETRIC,
                             INTENT_ABSOLUTE_COLORIMETRIC,
                             INTENT_SATURATION,
                             INTENT_PERCEPTUAL};

const pystd2025::Span<int> ri2lcms(ri2lcms_data, 4);

int32_t num_bytes_for(CapyPDF_Device_Colorspace cs) {
    switch(cs) {
    case CAPY_DEVICE_CS_RGB:
        return 3;
    case CAPY_DEVICE_CS_GRAY:
        return 1;
    case CAPY_DEVICE_CS_CMYK:
        return 4;
    }
    std::abort();
}

/*
int32_t num_bytes_for(CapyPDF_ImageColorspace cs) {
    switch(cs) {
    case CAPY_IMAGE_CS_RGB:
        return 3;
    case CAPY_IMAGE_CS_GRAY:
        return 1;
    case CAPY_IMAGE_CS_CMYK:
        return 4;
    }
    std::abort();
}
*/

uint32_t pixelformat_for(CapyPDF_Device_Colorspace cs) {
    switch(cs) {
    case CAPY_DEVICE_CS_RGB:
        return TYPE_RGB_8;
    case CAPY_DEVICE_CS_GRAY:
        return TYPE_GRAY_8;
    case CAPY_DEVICE_CS_CMYK:
        return TYPE_CMYK_8;
    }
    std::abort();
}

uint32_t pixelformat_for(CapyPDF_Image_Colorspace cs) {
    switch(cs) {
    case CAPY_IMAGE_CS_RGB:
        return TYPE_RGB_8;
    case CAPY_IMAGE_CS_GRAY:
        return TYPE_GRAY_8;
    case CAPY_IMAGE_CS_CMYK:
        return TYPE_CMYK_8;
    }
    std::abort();
}

void print_lcms_errors(cmsContext /*ContextID*/, cmsUInt32Number ErrorCode, const char *Text) {
    fprintf(stderr, "LCMS error: %d %s\n", (int)ErrorCode, Text);
}

} // namespace

namespace capypdf::internal {

LcmsHolder::~LcmsHolder() { deallocate(); }

void LcmsHolder::deallocate() {
    if(h) {
        cmsCloseProfile(h);
    }
    h = nullptr;
}

rvoe<PdfColorConverter> PdfColorConverter::construct(const pystd2025::Path &rgb_profile_fname,
                                                     const pystd2025::Path &gray_profile_fname,
                                                     const pystd2025::Path &cmyk_profile_fname) {
    PdfColorConverter conv;
    if(!rgb_profile_fname.is_empty()) {
        ERC(rgb, load_file_as_bytes(rgb_profile_fname));
        conv.rgb_profile_data = std::move(rgb);
        cmsHPROFILE h =
            cmsOpenProfileFromMem(conv.rgb_profile_data.data(), conv.rgb_profile_data.size());
        if(!h) {
            RETERR(InvalidICCProfile);
        }
        conv.rgb_profile.h = h;
        auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
        if(num_channels != 3) {
            RETERR(IncorrectColorChannelCount);
        }
    } else {
        conv.rgb_profile.h = cmsCreate_sRGBProfile();
    }
    if(!gray_profile_fname.is_empty()) {
        ERC(gray, load_file_as_bytes(gray_profile_fname));
        conv.gray_profile_data = std::move(gray);
        auto h =
            cmsOpenProfileFromMem(conv.gray_profile_data.data(), conv.gray_profile_data.size());
        if(!h) {
            RETERR(InvalidICCProfile);
        }
        conv.gray_profile.h = h;
        auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
        if(num_channels != 1) {
            RETERR(IncorrectColorChannelCount);
        }
    } else {
        auto curve = cmsBuildGamma(nullptr, 1.0);
        conv.gray_profile.h = cmsCreateGrayProfile(cmsD50_xyY(), curve);
        cmsFreeToneCurve(curve);
    }
    if(!cmyk_profile_fname.is_empty()) {
        ERC(cmyk, load_file_as_bytes(cmyk_profile_fname));
        conv.cmyk_profile_data = std::move(cmyk);
        auto h =
            cmsOpenProfileFromMem(conv.cmyk_profile_data.data(), conv.cmyk_profile_data.size());
        if(!h) {
            RETERR(InvalidICCProfile);
        }
        conv.cmyk_profile.h = h;
        auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
        if(num_channels != 4) {
            RETERR(IncorrectColorChannelCount);
        }
    } else {
        // Not having a CMYK profile is fine, but any call to CMYK color conversions
        // is an error.
    }
    cmsSetLogErrorHandler(print_lcms_errors);
    return rvoe<PdfColorConverter>(std::move(conv));
}

PdfColorConverter::PdfColorConverter() {}

PdfColorConverter::~PdfColorConverter() {}

rvoe<DeviceRGBColor> PdfColorConverter::to_rgb(const DeviceCMYKColor &cmyk) {
    if(!cmyk_profile.h) {
        RETERR(OutputProfileMissing);
    }
    assert(rgb_profile.h);
    DeviceRGBColor rgb;
    auto transform = cmsCreateTransform(cmyk_profile.h,
                                        TYPE_CMYK_DBL,
                                        rgb_profile.h,
                                        TYPE_RGB_DBL,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, &cmyk, &rgb, 1);
    cmsDeleteTransform(transform);
    return rgb;
}

DeviceGrayColor PdfColorConverter::to_gray(const DeviceRGBColor &rgb) {
    DeviceGrayColor gray;
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_DBL,
                                        gray_profile.h,
                                        TYPE_GRAY_DBL,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, &rgb, &gray, 1);
    cmsDeleteTransform(transform);
    return gray;
}

rvoe<DeviceGrayColor> PdfColorConverter::to_gray(const DeviceCMYKColor &cmyk) {
    if(!cmyk_profile.h) {
        RETERR(OutputProfileMissing);
    }
    assert(gray_profile.h);
    DeviceGrayColor gray;
    auto transform = cmsCreateTransform(cmyk_profile.h,
                                        TYPE_CMYK_DBL,
                                        gray_profile.h,
                                        TYPE_GRAY_DBL,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, &cmyk, &gray, 1);
    cmsDeleteTransform(transform);
    return gray;
}

rvoe<DeviceCMYKColor> PdfColorConverter::to_cmyk(const DeviceRGBColor &rgb) {
    if(!cmyk_profile.h) {
        RETERR(NoCmykProfile);
    }
    assert(rgb_profile.h);
    DeviceCMYKColor cmyk;
    double buf[4]; // PDF uses values [0, 1] but littlecms seems to use [0, 100].
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_DBL,
                                        cmyk_profile.h,
                                        TYPE_CMYK_DBL,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, &rgb, &buf, 1);
    cmyk.c = buf[0] / 100.0;
    cmyk.m = buf[1] / 100.0;
    cmyk.y = buf[2] / 100.0;
    cmyk.k = buf[3] / 100.0;
    cmsDeleteTransform(transform);
    return cmyk;
}

cmsHPROFILE PdfColorConverter::profile_for(CapyPDF_Device_Colorspace cs) const {
    switch(cs) {
    case CAPY_DEVICE_CS_RGB:
        return rgb_profile.h;
    case CAPY_DEVICE_CS_GRAY:
        return gray_profile.h;
    case CAPY_DEVICE_CS_CMYK:
        return cmyk_profile.h;
    }
    std::abort();
}

cmsHPROFILE PdfColorConverter::profile_for(CapyPDF_Image_Colorspace cs) const {
    if(cs <= CAPY_IMAGE_CS_CMYK) {
        return profile_for((CapyPDF_Device_Colorspace)cs);
    }
    std::abort();
}

rvoe<RawPixelImage> PdfColorConverter::convert_image_to(RawPixelImage ri,
                                                        CapyPDF_Device_Colorspace output_format,
                                                        CapyPDF_Rendering_Intent intent) const {
    RawPixelImage converted;
    converted.md = ri.md;
    converted.alpha = std::move(ri.alpha);
    cmsHPROFILE input_profile;
    const uint32_t input_pixelformat = pixelformat_for(ri.md.cs);
    const uint32_t output_pixelformat = pixelformat_for(output_format);
    assert(ri.md.w > 0);
    assert(ri.md.h > 0);
    const uint64_t num_pixels = ((uint64_t)ri.md.w) * ri.md.h;
    LcmsHolder icc_holder;
    if(ri.icc_profile.is_empty()) {
        input_profile = profile_for(ri.md.cs);
    } else {
        input_profile = cmsOpenProfileFromMem(ri.icc_profile.data(), ri.icc_profile.size());
        if(!input_profile) {
            RETERR(InvalidICCProfile);
        }
        icc_holder.h = input_profile;
    }

    if(!input_profile) {
        RETERR(InputProfileUnknown);
    }
    cmsHPROFILE output_profile = profile_for(output_format);
    if(!output_profile) {
        RETERR(OutputProfileMissing);
    }

    auto transform = cmsCreateTransform(input_profile,
                                        input_pixelformat,
                                        output_profile,
                                        output_pixelformat,
                                        ri2lcms.at(intent),
                                        0);
    if(!transform) {
        RETERR(ProfileProblem);
    }
    converted.pixels = pystd2025::Bytes(num_pixels * num_bytes_for(output_format), 0);
    cmsDoTransform(transform, ri.pixels.data(), converted.pixels.data(), num_pixels);
    cmsDeleteTransform(transform);
    converted.md.cs = (CapyPDF_Image_Colorspace)output_format;
    converted.icc_profile.clear();
    return converted;
}

pystd2025::BytesView PdfColorConverter::get_rgb() {
    return pystd2025::BytesView(rgb_profile_data.data(), rgb_profile_data.size());
}
pystd2025::BytesView PdfColorConverter::get_gray() {
    return pystd2025::BytesView(gray_profile_data.data(), gray_profile_data.size());
}
pystd2025::BytesView PdfColorConverter::get_cmyk() {
    return pystd2025::BytesView(cmyk_profile_data.data(), cmyk_profile_data.size());
}

rvoe<int> PdfColorConverter::get_num_channels(pystd2025::BytesView icc_data) const {
    cmsHPROFILE h = cmsOpenProfileFromMem(icc_data.data(), icc_data.size());
    if(!h) {
        RETERR(InvalidICCProfile);
    }
    auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    cmsCloseProfile(h);
    return num_channels;
}

} // namespace capypdf::internal
