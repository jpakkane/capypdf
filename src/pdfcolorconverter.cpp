// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <pdfcolorconverter.hpp>
#include <string_view>
#include <utils.hpp>
#include <expected>
// https://github.com/mm2/Little-CMS/pull/403
#define CMS_NO_REGISTER_KEYWORD
#include <lcms2.h>
#include <cassert>
#include <stdexcept>
#include <array>

namespace {

const std::array<int, 4> ri2lcms = {INTENT_RELATIVE_COLORIMETRIC,
                                    INTENT_ABSOLUTE_COLORIMETRIC,
                                    INTENT_SATURATION,
                                    INTENT_PERCEPTUAL};

}

namespace capypdf {

LcmsHolder::~LcmsHolder() { deallocate(); }

void LcmsHolder::deallocate() {
    if(h) {
        cmsCloseProfile(h);
    }
    h = nullptr;
}

rvoe<PdfColorConverter>
PdfColorConverter::construct(const std::filesystem::path &rgb_profile_fname,
                             const std::filesystem::path &gray_profile_fname,
                             const std::filesystem::path &cmyk_profile_fname) {
    PdfColorConverter conv;
    if(!rgb_profile_fname.empty()) {
        ERC(rgb, load_file(rgb_profile_fname));
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
    if(!gray_profile_fname.empty()) {
        ERC(gray, load_file(gray_profile_fname));
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
    if(!cmyk_profile_fname.empty()) {
        ERC(cmyk, load_file(cmyk_profile_fname));
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
    return rvoe<PdfColorConverter>(std::move(conv));
}

PdfColorConverter::PdfColorConverter() {}

PdfColorConverter::~PdfColorConverter() {}

DeviceRGBColor PdfColorConverter::to_rgb(const DeviceCMYKColor &cmyk) {
    DeviceRGBColor rgb;
    auto transform = cmsCreateTransform(cmyk_profile.h,
                                        TYPE_CMYK_DBL,
                                        gray_profile.h,
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

DeviceGrayColor PdfColorConverter::to_gray(const DeviceCMYKColor &cmyk) {
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

std::string PdfColorConverter::rgb_pixels_to_gray(std::string_view rgb_data) {
    assert(rgb_data.size() % 3 == 0);
    const int32_t num_pixels = (int32_t)rgb_data.size() / 3;
    std::string converted_pixels(num_pixels, '\0');
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_8,
                                        gray_profile.h,
                                        TYPE_GRAY_8,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, rgb_data.data(), converted_pixels.data(), num_pixels);
    cmsDeleteTransform(transform);
    return converted_pixels;
}

rvoe<std::string> PdfColorConverter::rgb_pixels_to_cmyk(std::string_view rgb_data) {
    if(!cmyk_profile.h) {
        RETERR(NoCmykProfile);
    }
    assert(rgb_data.size() % 3 == 0);
    const int32_t num_pixels = (int32_t)rgb_data.size() / 3;
    std::string converted_pixels(num_pixels * 4, '\0');
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_8,
                                        cmyk_profile.h,
                                        TYPE_CMYK_8,
                                        ri2lcms.at(CAPY_RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, rgb_data.data(), converted_pixels.data(), num_pixels);
    cmsDeleteTransform(transform);
    return converted_pixels;
}

rvoe<int> PdfColorConverter::get_num_channels(std::string_view icc_data) const {
    cmsHPROFILE h = cmsOpenProfileFromMem(icc_data.data(), icc_data.size());
    if(!h) {
        RETERR(InvalidICCProfile);
    }
    auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    cmsCloseProfile(h);
    return num_channels;
}

} // namespace capypdf
