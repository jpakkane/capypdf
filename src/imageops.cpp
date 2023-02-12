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

#include <imageops.hpp>
#include <utils.hpp>
#include <png.h>
#include <jpeglib.h>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <memory>

namespace A4PDF {

namespace {

rgb_image load_rgb_png(png_image &image) {
    rgb_image result;
    result.w = image.width;
    result.h = image.height;
    result.pixels.resize(PNG_IMAGE_SIZE(image));

    png_image_finish_read(
        &image, NULL, result.pixels.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
    if(PNG_IMAGE_FAILED(image)) {
        std::string msg("PNG file reading failed: ");
        msg += image.message;
        throw std::runtime_error(std::move(msg));
    }
    return result;
}

rgb_image load_rgba_png(png_image &image) {
    rgb_image result;
    std::string buf;
    result.w = image.width;
    result.h = image.height;
    buf.resize(PNG_IMAGE_SIZE(image));
    result.alpha = std::string();

    png_image_finish_read(&image, NULL, buf.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
    if(PNG_IMAGE_FAILED(image)) {
        std::string msg("PNG file reading failed: ");
        msg += image.message;
        throw std::runtime_error(std::move(msg));
    }
    assert(buf.size() % 4 == 0);
    result.pixels.reserve(PNG_IMAGE_SIZE(image) * 3 / 4);
    result.alpha->reserve(PNG_IMAGE_SIZE(image) / 4);
    for(size_t i = 0; i < buf.size(); i += 4) {
        result.pixels += buf[i];
        result.pixels += buf[i + 1];
        result.pixels += buf[i + 2];
        *result.alpha += buf[i + 3];
    }

    return result;
}

} // namespace

rgb_image load_image_file(const char *fname) {
    png_image image;
    std::unique_ptr<png_image, decltype(&png_image_free)> pngcloser(&image, &png_image_free);

    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if(png_image_begin_read_from_file(&image, fname) != 0) {
        if(image.format == PNG_FORMAT_RGBA) {
            return load_rgba_png(image);
        } else if(image.format == PNG_FORMAT_RGB) {
            return load_rgb_png(image);
        } else {
            throw std::runtime_error("Only RGB images supported.");
        }
    } else {
        std::string msg("Opening a PNG file failed: ");
        msg += image.message;
        throw std::runtime_error(std::move(msg));
    }
    throw std::runtime_error("Unreachable code.");
}

jpg_image load_jpg(const char *fname) {
    jpg_image im;
    im.file_contents = load_file(fname);
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    std::unique_ptr<jpeg_decompress_struct, decltype(&jpeg_destroy_decompress)> jpgcloser(
        &cinfo, &jpeg_destroy_decompress);

    jpeg_mem_src(&cinfo, (const unsigned char *)im.file_contents.data(), im.file_contents.length());
    if(jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        throw std::runtime_error(std::string{"Not a valid jpg file: "} + fname);
    }
    jpeg_start_decompress(&cinfo);
    im.h = cinfo.image_height;
    im.w = cinfo.image_width;
    // Fixme, validate that this is an 8bpp RGB image.
    return im;
}

} // namespace A4PDF
