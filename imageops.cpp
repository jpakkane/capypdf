#include <imageops.hpp>
#include <png.h>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <memory>
#include <cassert>

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
