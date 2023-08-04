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

#include <filesystem>
#include <imageops.hpp>
#include <utils.hpp>
#include <png.h>
#include <jpeglib.h>
#include <tiffio.h>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <memory>

namespace capypdf {

namespace {

std::expected<rgb_image, ErrorCode> load_rgb_png(png_image &image) {
    rgb_image result;
    result.w = image.width;
    result.h = image.height;
    result.pixels.resize(PNG_IMAGE_SIZE(image));

    png_image_finish_read(
        &image, NULL, result.pixels.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
    if(PNG_IMAGE_FAILED(image)) {
        fprintf(stderr, "%s\n", image.message);
        RETERR(UnsupportedFormat);
    }
    return result;
}

std::expected<rgb_image, ErrorCode> load_rgba_png(png_image &image) {
    rgb_image result;
    std::string buf;
    result.w = image.width;
    result.h = image.height;
    buf.resize(PNG_IMAGE_SIZE(image));
    result.alpha = std::string();

    png_image_finish_read(&image, NULL, buf.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
    if(PNG_IMAGE_FAILED(image)) {
        fprintf(stderr, "%s\n", image.message);
        RETERR(UnsupportedFormat);
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

    return std::move(result);
}

std::expected<gray_image, ErrorCode> load_ga_png(png_image &image) {
    gray_image result;
    std::string buf;
    result.w = image.width;
    result.h = image.height;
    buf.resize(PNG_IMAGE_SIZE(image));
    result.alpha = std::string();

    png_image_finish_read(&image, NULL, buf.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
    if(PNG_IMAGE_FAILED(image)) {
        fprintf(stderr, "%s\n", image.message);
        RETERR(UnsupportedFormat);
    }
    result.pixels.reserve(PNG_IMAGE_SIZE(image) / 2);
    result.alpha->reserve(PNG_IMAGE_SIZE(image) / 2);
    for(size_t i = 0; i < buf.size(); i += 2) {
        result.pixels += buf[i];
        *result.alpha += buf[i + 1];
    }

    return std::move(result);
}

struct png_data {
    std::string pixels;
    std::string colormap;
};

std::expected<png_data, ErrorCode> load_png_data(png_image &image) {
    png_data pd;

    auto bufsize = PNG_IMAGE_SIZE(image);
    pd.pixels.resize(bufsize);
    pd.colormap.resize(PNG_IMAGE_COLORMAP_SIZE(image));
    png_image_finish_read(
        &image, NULL, pd.pixels.data(), PNG_IMAGE_SIZE(image) / image.height, pd.colormap.data());
    if(PNG_IMAGE_FAILED(image)) {
        fprintf(stderr, "%s\n", image.message);
        RETERR(UnsupportedFormat);
    }
    return std::move(pd);
}

std::expected<mono_image, ErrorCode> load_mono_png(png_image &image) {
    mono_image result;
    const size_t final_size = (image.width + 7) / 8 * image.height;
    result.pixels.reserve(final_size);
    result.w = image.width;
    result.h = image.height;
    ERC(pd, load_png_data(image));
    size_t offset = 0;
    const int white_pixel = pd.colormap[0] == 1 ? 1 : 0;
    const int num_padding_bits = (result.w % 8) == 0 ? 0 : 8 - result.w % 8;
    for(int j = 0; j < result.h; ++j) {
        unsigned char current_byte = 0;
        for(int i = 0; i < result.w; ++i) {
            current_byte <<= 1;
            if(pd.pixels[offset] == white_pixel) {
                current_byte |= 1;
            }
            if((i % 8 == 0) && i > 0) {
                result.pixels.push_back(~current_byte);
                current_byte = 0;
            }
            ++offset;
        }
        // PDF spec 8.9.3 "Sample representation"

        if(num_padding_bits > 0) {
            current_byte <<= num_padding_bits;
        }
        result.pixels.push_back(~current_byte);
    }
    assert(result.pixels.size() == final_size);
    return std::move(result);
}

struct pngbytes {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

bool is_1bit(std::string_view colormap) {
    for(size_t off = 0; off < colormap.size(); off += sizeof(pngbytes)) {
        const pngbytes *e = reinterpret_cast<const pngbytes *>(colormap.data() + off);
        if(e->a == 255 || e->a == 0) {
        } else {
            return false;
        }
        if(e->r == 0 && e->g == 0 && e->b == 0) {
            continue;
        }
        if(e->r == 255 && e->g == 255 && e->b == 255) {
            continue;
        }
        return false;
    }
    return true;
}

// Special case for images that have 1-bit monochrome colors and a 1-bit alpha channel.
std::expected<std::optional<mono_image>, ErrorCode> try_load_mono_alpha_png(png_image &image) {
    ERC(pd, load_png_data(image));
    if(!is_1bit(pd.colormap)) {
        return std::optional<mono_image>{};
    }
    mono_image result;
    result.w = image.width;
    result.h = image.height;
    result.alpha = std::string{};
    const size_t final_size = (image.width + 7) / 8 * image.height;
    result.pixels.reserve(final_size);
    result.alpha->reserve(final_size);
    const uint8_t black_pixel = 0;
    const uint8_t transparent = 255;
    const int num_padding_bits = (result.w % 8) == 0 ? 0 : 8 - result.w % 8;
    for(int j = 0; j < result.h; ++j) {
        unsigned char current_byte = 0;
        unsigned char current_mask_byte = 255;
        for(int i = 0; i < result.w; ++i) {
            current_byte <<= 1;
            current_mask_byte <<= 1;
            const auto colormap_entry = pd.pixels.at(j * result.w + i);
            assert(colormap_entry * sizeof(pngbytes) < pd.colormap.size());
            if(colormap_entry != 0) {
                ++i;
                --i;
            }
            const auto *colormap_data = pd.colormap.data() + colormap_entry * sizeof(pngbytes);
            const auto *pixel = reinterpret_cast<const pngbytes *>(colormap_data);
            if(pixel->r == black_pixel) {
                current_byte |= 1;
            }
            if(pixel->a != transparent) {
                current_mask_byte |= 1;
            }
            if((i % 8 == 0) && i > 0) {
                result.pixels.push_back(~current_byte);
                result.alpha->push_back(~current_mask_byte);
                current_byte = 0;
                current_mask_byte = 255;
            }
        }
        // PDF spec 8.9.3 "Sample representation"

        if(num_padding_bits > 0) {
            current_byte <<= num_padding_bits;
            current_mask_byte <<= num_padding_bits;
        }
        result.pixels.push_back(current_byte);
        result.alpha->push_back(~current_mask_byte);
    }
    assert(result.pixels.size() == final_size);
    assert(result.alpha->size() == final_size);
    return std::move(result);
}

std::expected<RasterImage, ErrorCode> load_png_file(const std::filesystem::path &fname) {
    png_image image;
    std::unique_ptr<png_image, decltype(&png_image_free)> pngcloser(&image, &png_image_free);

    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if(png_image_begin_read_from_file(&image, fname.string().c_str()) != 0) {
        if(image.format == PNG_FORMAT_RGBA) {
            return load_rgba_png(image);
        } else if(image.format == PNG_FORMAT_RGB) {
            return load_rgb_png(image);
        } else if(image.format == PNG_FORMAT_GA) {
            return load_ga_png(image);
        } else if(image.format & PNG_FORMAT_FLAG_COLORMAP) {
            if(!(image.format & PNG_FORMAT_FLAG_COLOR)) {
                RETERR(UnsupportedFormat);
            }
            if(image.colormap_entries == 2) {
                return load_mono_png(image);
            }
            if(image.colormap_entries == 3 || image.colormap_entries == 4) {
                ERC(res, try_load_mono_alpha_png(image));
                if(res) {
                    return std::move(*res);
                }
            }
            RETERR(NonBWColormap);
        } else {
            RETERR(UnsupportedFormat);
        }
    } else {
        fprintf(stderr, "%s\n", image.message);
        RETERR(UnsupportedFormat);
    }
    RETERR(Unreachable);
}

rvoe<RasterImage> load_tif_file(const std::filesystem::path &fname) {
    TIFF *tif = TIFFOpen(fname.string().c_str(), "rb");
    if(!tif) {
        RETERR(FileReadError);
    }
    std::optional<std::string> icc;
    std::unique_ptr<TIFF, decltype(&TIFFClose)> tiffcloser(tif, TIFFClose);

    uint32_t w{}, h{};
    uint16_t bitspersample{}, samplesperpixel{}, photometric{}, planarconf{};
    uint32_t icc_count{};
    void *icc_data{};

    if(TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w) != 1) {
        RETERR(UnsupportedTIFF);
    }
    if(TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h) != 1) {
        RETERR(UnsupportedTIFF);
    }

    if(TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample) != 1) {
        RETERR(UnsupportedTIFF);
    }
    if(bitspersample != 8) {
        RETERR(UnsupportedTIFF);
    }

    if(TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel) != 1) {
        RETERR(UnsupportedTIFF);
    }

    if(TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric) != 1) {
        RETERR(UnsupportedTIFF);
    }

    /*
     * Note that the output variable is an array, because there can
     * be more than 1 extra channel.
    if(TIFFGetField(tif, TIFFTAG_EXTRASAMPLES, &extrasamples) == 1) {
        if(extrasamples != 0) {
            fprintf(stderr, "TIFFs with an alpha channel not supported yet.");
            RETERR(UnsupportedTIFF);
        }
    }
    */

    if(TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconf) != 1) {
        RETERR(UnsupportedTIFF);
    }
    // Maybe fail for this?

    if(TIFFGetField(tif, TIFFTAG_ICCPROFILE, &icc_count, &icc_data) == 1) {
        icc = std::string{(const char *)icc_data, icc_count};
    }

    const auto scanlinesize = TIFFScanlineSize64(tif);
    std::string line(scanlinesize, 0);
    std::string pixels;
    pixels.reserve(scanlinesize * h);
    for(uint32_t row = 0; row < h; ++row) {
        if(TIFFReadScanline(tif, line.data(), row) != 1) {
            fprintf(stderr, "TIFF decoding failed.\n");
            RETERR(FileReadError);
        }
        pixels += line;
    }
    switch(photometric) {
    case PHOTOMETRIC_SEPARATED:
        if(samplesperpixel != 4) {
            RETERR(UnsupportedTIFF);
        }
        return cmyk_image{(int32_t)w, (int32_t)h, std::move(pixels), std::move(icc)};
    case PHOTOMETRIC_RGB:
        if(samplesperpixel != 3) {
            RETERR(UnsupportedTIFF);
        }
        return rgb_image{(int32_t)w, (int32_t)h, std::move(pixels), std::move(icc)};
    case PHOTOMETRIC_MINISBLACK:
        if(samplesperpixel != 1) {
            RETERR(UnsupportedTIFF);
        }
        return gray_image{(int32_t)w, (int32_t)h, std::move(pixels), std::move(icc)};

    default:
        RETERR(UnsupportedTIFF);
    }
}

} // namespace

rvoe<jpg_image> load_jpg(const std::filesystem::path &fname) {
    jpg_image im;
    ERC(contents, load_file(fname));
    im.file_contents = std::move(contents);
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    std::unique_ptr<jpeg_decompress_struct, decltype(&jpeg_destroy_decompress)> jpgcloser(
        &cinfo, &jpeg_destroy_decompress);

    jpeg_mem_src(&cinfo, (const unsigned char *)im.file_contents.data(), im.file_contents.length());
    if(jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        RETERR(UnsupportedFormat);
    }
    jpeg_start_decompress(&cinfo);
    im.h = cinfo.image_height;
    im.w = cinfo.image_width;
    // Fixme, validate that this is an 8bpp RGB image.
    return std::move(im);
}

rvoe<RasterImage> load_image_file(const std::filesystem::path &fname) {
    auto extension = fname.extension();
    if(extension == ".png" || extension == ".PNG") {
        return load_png_file(fname);
    }
    if(extension == ".tif" || extension == ".tiff" || extension == ".TIF" || extension == ".TIFF") {
        return load_tif_file(fname);
    }
    fprintf(stderr, "Unsupported image file format: %s\n", fname.string().c_str());
    RETERR(UnsupportedFormat);
}

} // namespace capypdf
