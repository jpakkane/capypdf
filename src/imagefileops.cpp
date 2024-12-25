// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <filesystem>
#include <imagefileops.hpp>
#include <utils.hpp>
#include <png.h>
#include <jpeglib.h>
#include <tiffio.h>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <memory>
#include <span>

namespace capypdf::internal {

namespace {

void load_rgb_png(png_struct *png_ptr, png_info *info_ptr, RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    result.md.pixel_depth = 8;
    result.md.cs = CAPY_IMAGE_CS_RGB;
    result.pixels.reserve(result.md.w * result.md.h * 3);

    for(uint32_t row_number = 0; row_number < result.md.h; ++row_number) {
        unsigned char *current_row = rows[row_number];
        for(size_t i = 0; i < result.md.w * 3; i += 3) {
            result.pixels += current_row[i];
            result.pixels += current_row[i + 1];
            result.pixels += current_row[i + 2];
        }
    }
}

void load_rgba_png(png_struct *png_ptr, png_info *info_ptr, RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    result.md.pixel_depth = 8;
    result.md.alpha_depth = 8;
    result.md.cs = CAPY_IMAGE_CS_RGB;

    result.pixels.reserve(result.md.w * result.md.h * 3);
    result.alpha.reserve(result.md.w * result.md.h);

    for(uint32_t row_number = 0; row_number < result.md.h; ++row_number) {
        unsigned char *current_row = rows[row_number];
        for(size_t i = 0; i < result.md.w * 4; i += 4) {
            result.pixels += current_row[i];
            result.pixels += current_row[i + 1];
            result.pixels += current_row[i + 2];
            result.alpha += current_row[i + 3];
        }
    }
}

void load_gray_png(png_struct *png_ptr, png_info *info_ptr, RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    result.md.pixel_depth = 8;
    result.md.cs = CAPY_IMAGE_CS_GRAY;

    result.pixels.reserve(result.md.w * result.md.h);

    for(uint32_t row_number = 0; row_number < result.md.h; ++row_number) {
        unsigned char *current_row = rows[row_number];
        for(size_t i = 0; i < result.md.w; ++i) {
            result.pixels += current_row[i];
        }
    }
}

void load_ga_png(png_struct *png_ptr, png_info *info_ptr, RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    result.md.pixel_depth = 8;
    result.md.alpha_depth = 8;
    result.md.cs = CAPY_IMAGE_CS_GRAY;

    result.pixels.reserve(result.md.w * result.md.h);
    result.alpha.reserve(result.md.w * result.md.h);
    for(uint32_t row_number = 0; row_number < result.md.h; ++row_number) {
        unsigned char *current_row = rows[row_number];
        for(size_t i = 0; i < result.md.w * 2; i += 2) {
            result.pixels += current_row[i];
            result.alpha += current_row[i + 1];
        }
    }
}

bool is_white(const png_color *c) { return c->red == 255 && c->green == 255 && c->blue == 255; }
bool is_white(const png_color &c) { return is_white(&c); }

void load_mono_png(png_struct *png_ptr,
                   png_info *info_ptr,
                   std::span<png_color> palette,
                   RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    const size_t final_size = (result.md.w + 7) / 8 * result.md.h;
    result.pixels.reserve(final_size);
    result.md.pixel_depth = 1;
    result.md.cs = CAPY_IMAGE_CS_GRAY;
    const int num_padding_bits = (result.md.w % 8) == 0 ? 0 : 8 - result.md.w % 8;
    const int white_pixel = is_white(palette[0]) ? 0 : 1;
    for(uint32_t j = 0; j < result.md.h; ++j) {
        unsigned char current_byte = 0;
        auto current_row = rows[j];
        for(uint32_t i = 0; i < result.md.w; ++i) {
            current_byte <<= 1;
            if(current_row[i] != white_pixel) {
                current_byte |= 1;
            }
            if((i % 8 == 0) && i > 0) {
                result.pixels.push_back(~current_byte);
                current_byte = 0;
            }
        }
        // PDF spec 8.9.3 "Sample representation"

        if(num_padding_bits > 0) {
            current_byte <<= num_padding_bits;
        }
        result.pixels.push_back(~current_byte);
    }
    assert(result.pixels.size() == final_size);
}

template<typename T> bool span_contains(std::span<T> &span, const T &value) {
    for(const auto &e : span) {
        if(e == value) {
            return true;
        }
    }
    return false;
}

// Special case for images that have 1-bit monochrome colors and a 1-bit alpha channel.
rvoe<NoReturnValue> try_load_mono_alpha_png(png_struct *png_ptr,
                                            png_info *info_ptr,
                                            std::span<png_color> palette,
                                            std::span<unsigned char> alpha,
                                            RawPixelImage &result) {
    unsigned char **rows = png_get_rows(png_ptr, info_ptr);
    result.md.pixel_depth = 1;
    result.md.alpha_depth = 1;
    result.md.cs = CAPY_IMAGE_CS_GRAY;
    const size_t final_size = (result.md.w + 7) / 8 * result.md.h;
    result.pixels.reserve(final_size);
    result.alpha.reserve(final_size);
    const int num_padding_bits = (result.md.w % 8) == 0 ? 0 : 8 - result.md.w % 8;
    for(uint32_t j = 0; j < result.md.h; ++j) {
        unsigned char current_byte = 0;
        unsigned char current_mask_byte = 255;
        auto *current_row = rows[j];
        for(uint32_t i = 0; i < result.md.w; ++i) {
            current_byte <<= 1;
            current_mask_byte <<= 1;
            auto input_value = current_row[i];
            assert(input_value < palette.size()); // Span has .at only at c++26.
            bool is_white_p = is_white(palette[input_value]);
            bool is_trans_p = span_contains(alpha, current_row[i]);
            if(!is_white_p) {
                current_byte |= 1;
            }
            if(is_trans_p) {
                current_mask_byte |= 1;
            }
            if((i % 8 == 0) && i > 0) {
                result.pixels.push_back(~current_byte);
                result.alpha.push_back(~current_mask_byte);
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
        result.alpha.push_back(~current_mask_byte);
    }
    assert(result.pixels.size() == final_size);
    assert(result.alpha.size() == final_size);
    return NoReturnValue{};
}

rvoe<RasterImage> do_png_load(png_struct *png_ptr, png_info *info_ptr) {
    RawPixelImage image;
    image.md.w = png_get_image_width(png_ptr, info_ptr);
    image.md.h = png_get_image_height(png_ptr, info_ptr);
    char *profile_name;
    int compression_type = PNG_COMPRESSION_TYPE_BASE;
    unsigned char *icc;
    uint32_t icc_length;
    if(png_get_iCCP(png_ptr, info_ptr, &profile_name, &compression_type, &icc, &icc_length) ==
       PNG_INFO_iCCP) {
        std::byte *icc_data = (std::byte *)(icc);
        image.icc_profile = std::vector<std::byte>(icc_data, icc_data + icc_length);
    }
    const auto image_format = png_get_color_type(png_ptr, info_ptr);
    if(image_format == PNG_COLOR_TYPE_RGB_ALPHA) {
        load_rgba_png(png_ptr, info_ptr, image);
    } else if(image_format == PNG_COLOR_TYPE_RGB) {
        load_rgb_png(png_ptr, info_ptr, image);
    } else if(image_format == PNG_COLOR_TYPE_GRAY) {
        load_gray_png(png_ptr, info_ptr, image);
    } else if(image_format == PNG_COLOR_TYPE_GRAY_ALPHA) {
        load_ga_png(png_ptr, info_ptr, image);
    } else if(image_format == PNG_COLOR_TYPE_PALETTE) {
        png_color *palette;
        int palette_size;
        png_get_PLTE(png_ptr, info_ptr, &palette, &palette_size);
        std::span<png_color> pspan(palette, palette_size);
        unsigned char *trans;
        int num_trans;
        png_color_16p trcolor;
        auto trns_rc = png_get_tRNS(png_ptr, info_ptr, &trans, &num_trans, &trcolor);
        const int32_t num_channels = png_get_channels(png_ptr, info_ptr);
        if(num_channels != 1) {
            RETERR(UnsupportedFormat);
        }
        if(palette_size == 2) {
            // FIXME: validate that the two entries in the file are in fact black and white.

            // Some programs write ICC profiles to monochrome images. They confuse
            // PDF renderers quite a bit so delete.
            image.icc_profile.clear();
            // white
            load_mono_png(png_ptr, info_ptr, pspan, image);
        } else if((palette_size == 3 || palette_size == 4) && trns_rc == PNG_INFO_tRNS) {
            std::span<unsigned char> aspan(trans, num_trans);
            image.icc_profile.clear();
            ERCV(try_load_mono_alpha_png(png_ptr, info_ptr, pspan, aspan, image));
        } else {
            RETERR(NonBWColormap);
        }
    } else {
        RETERR(UnsupportedFormat);
    }
    return image;
}

void separate_tif_alpha(RawPixelImage &image, const size_t num_color_channels) {
    assert(image.md.alpha_depth == 8);
    assert(image.pixels.size() % (num_color_channels + 1) == 0);
    assert(image.alpha.empty());
    std::string colors;
    colors.reserve(image.pixels.size() * num_color_channels / (num_color_channels + 1));
    image.alpha.reserve(image.pixels.size() / (num_color_channels + 1));
    for(size_t i = 0; i < image.pixels.size(); i += (num_color_channels + 1)) {
        for(size_t j = 0; j < num_color_channels; ++j) {
            colors.push_back(image.pixels[i + j]);
        }
        image.alpha.push_back(image.pixels[num_color_channels]);
    }
    image.pixels = std::move(colors);
}

rvoe<RasterImage> do_tiff_load(TIFF *tif) {
    RawPixelImage result;
    std::optional<std::string> icc;

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
        result.icc_profile = std::vector<std::byte>((const std::byte *)icc_data,
                                                    (const std::byte *)icc_data + icc_count);
    }

    result.md.pixel_depth = bitspersample;
    result.md.alpha_depth = bitspersample;

    const auto scanlinesize = TIFFScanlineSize64(tif);
    std::string line(scanlinesize, 0);
    result.pixels.reserve(scanlinesize * h);
    for(uint32_t row = 0; row < h; ++row) {
        if(TIFFReadScanline(tif, line.data(), row) != 1) {
            fprintf(stderr, "TIFF decoding failed.\n");
            RETERR(FileReadError);
        }
        result.pixels += line;
    }
    result.md.w = w;
    result.md.h = h;
    result.md.pixel_depth = 8;
    result.md.alpha_depth = 8;

    switch(photometric) {
    case PHOTOMETRIC_SEPARATED:
        if(samplesperpixel == 5) {
            separate_tif_alpha(result, 4);
        } else if(samplesperpixel != 4) {
            RETERR(UnsupportedTIFF);
        }
        result.md.cs = CAPY_IMAGE_CS_CMYK;
        break;

    case PHOTOMETRIC_RGB:
        if(samplesperpixel == 4) {
            separate_tif_alpha(result, 3);
        } else if(samplesperpixel != 3) {
            RETERR(UnsupportedTIFF);
        }
        result.md.cs = CAPY_IMAGE_CS_RGB;
        break;

    case PHOTOMETRIC_MINISBLACK:
        if(samplesperpixel == 2) {
            separate_tif_alpha(result, 1);
        } else if(samplesperpixel != 1) {
            RETERR(UnsupportedTIFF);
        }
        result.md.cs = CAPY_IMAGE_CS_GRAY;
        break;

    default:
        RETERR(UnsupportedTIFF);
    }
    return result;
}

struct TifBuf {
    const char *buf;
    int64_t bufsize;
    int64_t fptr;
};

tmsize_t tiffreadfunc(thandle_t h, void *buf, tmsize_t bufsize) {
    TifBuf *tiffdata = (TifBuf *)h;
    auto num_bytes = std::min(bufsize, tiffdata->bufsize - tiffdata->fptr);
    assert(num_bytes >= 0);
    memcpy(buf, tiffdata->buf + tiffdata->fptr, num_bytes);
    tiffdata->fptr += num_bytes;
    return num_bytes;
}

tmsize_t tiffwritefunc(thandle_t, void *, tmsize_t) { std::abort(); }

toff_t tiffseekfunc(thandle_t h, toff_t off, int whence) {
    TifBuf *tiffdata = (TifBuf *)h;
    switch(whence) {
    case SEEK_SET:
        tiffdata->fptr = off;
        break;
    case SEEK_CUR:
        tiffdata->fptr += off;
        break;
    case SEEK_END:
        tiffdata->fptr = tiffdata->bufsize + off;
        break;
    default:
        std::abort();
    }
    tiffdata->fptr = std::clamp(tiffdata->fptr, int64_t{0}, tiffdata->bufsize);
    return tiffdata->fptr;
};

int tiffclosefunc(thandle_t) { return 0; }

toff_t tiffsizefunc(thandle_t h) {
    TifBuf *tiffdata = (TifBuf *)h;
    return tiffdata->bufsize;
}

int tiffmmapfunc(thandle_t h, tdata_t *base, toff_t *psize) {
    TifBuf *tiffdata = (TifBuf *)h;
    *base = (void *)tiffdata->buf;
    *psize = tiffdata->bufsize;
    return 1;
}

void tiffunmapfunc(thandle_t, tdata_t, toff_t) {}

rvoe<RasterImage> load_tif_from_memory(const char *buf, int64_t bufsize) {
    assert(bufsize > 0);
    TifBuf tb{buf, bufsize, 0};
    TIFF *tif = TIFFClientOpen("memory",
                               "r",
                               (thandle_t)&tb,
                               tiffreadfunc,
                               tiffwritefunc,
                               tiffseekfunc,
                               tiffclosefunc,
                               tiffsizefunc,
                               tiffmmapfunc,
                               tiffunmapfunc);
    if(!tif) {
        RETERR(FileReadError);
    }
    std::unique_ptr<TIFF, decltype(&TIFFClose)> tiffcloser(tif, TIFFClose);
    return do_tiff_load(tif);
}

rvoe<RasterImage> load_png_file(FILE *f) {
    struct PngCloser {
        png_struct *p = nullptr;
        png_info *i = nullptr;

        ~PngCloser() {
            png_destroy_info_struct(p, &i);
            png_destroy_read_struct(&p, nullptr, nullptr);
        }
    };
    PngCloser pclose;
    pclose.p = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    pclose.i = png_create_info_struct(pclose.p);

    if(setjmp(png_jmpbuf(pclose.p))) {
        RETERR(UnsupportedFormat);
    }
    png_init_io(pclose.p, f);
    png_read_png(pclose.p, pclose.i, PNG_TRANSFORM_PACKING, nullptr);
    return do_png_load(pclose.p, pclose.i);
}

rvoe<RasterImage> load_png_file(const std::filesystem::path &fname) {
    FILE *f = fopen(fname.string().c_str(), "r");
    if(!f) {
        RETERR(CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    return load_png_file(f);
}

rvoe<RasterImage> load_png_from_memory(const char *buf, int64_t bufsize) {
    // libpng could _in theory_ load png files from memory, but their
    // callback function does not have a ctx argument. This means
    // using global variables which is not threadsafe in the least.
    // The internal implementation gets around this by stuffing its
    // state inside the png private pointer. We can't do that.
    FILE *f = tmpfile();
    if(!f) {
        RETERR(CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    fwrite(buf, 1, bufsize, f);
    fseek(f, 0, SEEK_SET);
    return load_png_file(f);
}

rvoe<RasterImage> load_tif_file(const std::filesystem::path &fname) {
    TIFF *tif = TIFFOpen(fname.string().c_str(), "rb");
    if(!tif) {
        RETERR(FileReadError);
    }
    std::unique_ptr<TIFF, decltype(&TIFFClose)> tiffcloser(tif, TIFFClose);
    return do_tiff_load(tif);
}

struct JpegError {
    struct jpeg_error_mgr jmgr;
    jmp_buf buf;
};

void jpegErrorExit(j_common_ptr cinfo) {
    JpegError *e = (JpegError *)cinfo->err;
    longjmp(e->buf, 1);
}

rvoe<jpg_image> load_jpg_metadata(FILE *f, const char *buf, int64_t bufsize) {
    assert(f == nullptr || buf == nullptr);
    jpg_image im;
    // Libjpeg kills the process on invalid input.
    // Changing the behaviour requires mucking about
    // with setjmp/longjmp.
    //
    // See: https://github.com/eiimage/detiq-t/blob/master/ImageIn/JpgImage.cpp

    struct jpeg_decompress_struct cinfo;
    JpegError jerr;

    cinfo.err = jpeg_std_error(&jerr.jmgr);
    jerr.jmgr.error_exit = jpegErrorExit;
    std::unique_ptr<jpeg_decompress_struct, decltype(&jpeg_destroy_decompress)> jpgcloser(
        &cinfo, &jpeg_destroy_decompress);
    if(setjmp(jerr.buf)) {
        RETERR(UnsupportedFormat);
    }
    jpeg_create_decompress(&cinfo);
    if(f) {
        jpeg_stdio_src(&cinfo, f);
    } else {
        jpeg_mem_src(&cinfo, (const unsigned char *)buf, bufsize);
    }
    // Required for ICC profile reading to work.
    jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);
    if(jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        RETERR(UnsupportedFormat);
    }

    jpeg_start_decompress(&cinfo);
    im.h = cinfo.image_height;
    im.w = cinfo.image_width;

    switch(cinfo.jpeg_color_space) {
    case JCS_GRAYSCALE:
        im.cs = CAPY_DEVICE_CS_GRAY;
        break;
    case JCS_RGB:
        im.cs = CAPY_DEVICE_CS_RGB;
        break;
    case JCS_YCbCr:
        im.cs = CAPY_DEVICE_CS_RGB;
        break;
    case JCS_CMYK:
        im.cs = CAPY_DEVICE_CS_CMYK;
        // FIXME: should detect whether to invert or not:
        // https://graphicdesign.stackexchange.com/questions/12894/cmyk-jpegs-extracted-from-pdf-appear-inverted
        im.invert_channels = true;
        break;
    case JCS_YCCK:
        im.cs = CAPY_DEVICE_CS_CMYK;
        im.invert_channels = true;
        break;
    default:
        RETERR(UnsupportedFormat);
    }

    im.depth = cinfo.data_precision;
    JOCTET *icc_buf = nullptr;
    unsigned int icc_len;
    if(jpeg_read_icc_profile(&cinfo, &icc_buf, &icc_len)) {
        std::byte *icc_ptr = (std::byte *)(icc_buf);
        im.icc_profile.assign(icc_ptr, icc_ptr + icc_len);
        free(icc_buf);
    }
    if(im.depth != 8) {
        RETERR(UnsupportedFormat);
    }
    return im;
}

rvoe<jpg_image> load_jpg_file(const std::filesystem::path &fname) {
    FILE *f = fopen(fname.string().c_str(), "rb");
    if(!f) {
        RETERR(FileDoesNotExist);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    ERC(meta, load_jpg_metadata(f, nullptr, 0));
    ERC(file_contents, load_file_as_bytes(f));
    meta.file_contents = std::move(file_contents);
    return meta;
}

rvoe<jpg_image> load_jpg_from_memory(const char *buf, int64_t bufsize) {
    ERC(meta, load_jpg_metadata(nullptr, buf, bufsize));
    auto *byteptr = (const std::byte *)(buf);
    meta.file_contents = std::vector<std::byte>(byteptr, byteptr + bufsize);
    return meta;
}

} // namespace

rvoe<RasterImage> load_image_file(const std::filesystem::path &fname) {
    if(!std::filesystem::exists(fname)) {
        RETERR(FileDoesNotExist);
    }
    auto extension = fname.extension();
    if(extension == ".png" || extension == ".PNG") {
        return load_png_file(fname);
    }
    if(extension == ".tif" || extension == ".tiff" || extension == ".TIF" || extension == ".TIFF") {
        return load_tif_file(fname);
    }
    if(extension == ".jpg" || extension == ".jpeg" || extension == ".JPG" || extension == ".JPEG") {
        return load_jpg_file(fname);
    }

    // If the input file was created with `tmpfile` or something similar, it might
    // not even have an extension at all. Try to open it with all decoders because
    // there is not much else we can do.
    if(auto rc = load_png_file(fname)) {
        return rc;
    }
    if(auto rc = load_jpg_file(fname)) {
        return rc;
    }
    auto rc = load_tif_file(fname);
    if(!rc) {
        fprintf(stderr, "Unsupported image file format: %s\n", fname.string().c_str());
    }
    return rc;
}

rvoe<RasterImage> load_image_from_memory(const char *buf, int64_t bufsize) {
    // There is no metadata telling us what the bytes represent.
    // Autodetect with magic numbers.

    if(bufsize < 10) {
        RETERR(UnsupportedFormat);
    }

    std::string_view v(buf, buf + bufsize);

    if(v.starts_with(".PNG")) {
        return load_png_from_memory(buf, bufsize);
    }
    if(v.starts_with("II*") || v.starts_with("MM")) {
        return load_tif_from_memory(buf, bufsize);
    }
    if((unsigned char)v[0] == 0xff && (unsigned char)v[1] == 0xd8 && (unsigned char)v[2] == 0xff) {
        return load_jpg_from_memory(buf, bufsize);
    }
    RETERR(UnsupportedFormat);
}

} // namespace capypdf::internal
