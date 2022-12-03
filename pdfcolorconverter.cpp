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

#include <pdfcolorconverter.hpp>
#include <imageops.hpp>
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

PdfColorConverter::PdfColorConverter(const char *rgb_profile_fname,
                                     const char *gray_profile_fname,
                                     const char *cmyk_profile_fname) {
    rgb_profile_data = load_file(rgb_profile_fname);
    gray_profile_data = load_file(gray_profile_fname);
    cmyk_profile_data = load_file(cmyk_profile_fname);
    cmsHPROFILE h = cmsOpenProfileFromMem(rgb_profile_data.data(), rgb_profile_data.size());
    if(!h) {
        throw std::runtime_error(std::string("Could not open RGB color profile ") +
                                 rgb_profile_fname);
    }
    auto num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    if(num_channels != 3) {
        cmsCloseProfile(h);
        throw std::runtime_error("RGB profile does not have exactly 3 channels.");
    }
    rgb_profile.h = h;

    h = cmsOpenProfileFromMem(gray_profile_data.data(), gray_profile_data.size());
    if(!h) {
        throw std::runtime_error(std::string("Could not open gray color profile ") +
                                 gray_profile_fname);
    }
    num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    if(num_channels != 1) {
        cmsCloseProfile(h);
        throw std::runtime_error("Gray profile does not have exactly 1 channels.");
    }
    gray_profile.h = h;

    h = cmsOpenProfileFromMem(cmyk_profile_data.data(), cmyk_profile_data.size());
    if(!h) {
        throw std::runtime_error(std::string("Could not open cmyk color profile ") +
                                 cmyk_profile_fname);
    }
    num_channels = (int32_t)cmsChannelsOf(cmsGetColorSpace(h));
    if(num_channels != 4) {
        cmsCloseProfile(h);
        throw std::runtime_error("CMYK profile does not have exactly 4 channels.");
    }
    cmyk_profile.h = h;
}

PdfColorConverter::~PdfColorConverter() {}

DeviceGrayColor PdfColorConverter::to_gray(const DeviceRGBColor &rgb) {
    DeviceGrayColor gray;
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_DBL,
                                        gray_profile.h,
                                        TYPE_GRAY_DBL,
                                        ri2lcms.at(RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, &rgb, &gray, 1);
    cmsDeleteTransform(transform);
    return gray;
}

DeviceCMYKColor PdfColorConverter::to_cmyk(const DeviceRGBColor &rgb) {
    DeviceCMYKColor cmyk;
    double buf[4]; // PDF uses values [0, 1] but littlecms seems to use [0, 100].
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_DBL,
                                        cmyk_profile.h,
                                        TYPE_CMYK_DBL,
                                        ri2lcms.at(RI_RELATIVE_COLORIMETRIC),
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
                                        ri2lcms.at(RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, rgb_data.data(), converted_pixels.data(), num_pixels);
    cmsDeleteTransform(transform);
    return converted_pixels;
}

std::string PdfColorConverter::rgb_pixels_to_cmyk(std::string_view rgb_data) {
    assert(rgb_data.size() % 3 == 0);
    const int32_t num_pixels = (int32_t)rgb_data.size() / 3;
    std::string converted_pixels(num_pixels * 4, '\0');
    auto transform = cmsCreateTransform(rgb_profile.h,
                                        TYPE_RGB_8,
                                        cmyk_profile.h,
                                        TYPE_CMYK_8,
                                        ri2lcms.at(RI_RELATIVE_COLORIMETRIC),
                                        0);
    cmsDoTransform(transform, rgb_data.data(), converted_pixels.data(), num_pixels);
    cmsDeleteTransform(transform);
    return converted_pixels;
}
