#include <imageops.hpp>
#include <png.h>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <memory>

rgb_image load_image_file(const char *fname) {
    png_image image;
    rgb_image result;
    std::unique_ptr<png_image, decltype(&png_image_free)> pngcloser(&image, &png_image_free);

    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if(png_image_begin_read_from_file(&image, fname) != 0) {
        if(image.format == PNG_FORMAT_RGBA) {
            if(image.format & PNG_FORMAT_FLAG_ALPHA) {
                throw std::runtime_error("Only RGB (not A) images supported.");
            }
            image.format = PNG_FORMAT_RGB;
        } else if(image.format != PNG_FORMAT_RGB) {
            throw std::runtime_error("Only RGB images supported.");
        }
        result.pixels.resize(PNG_IMAGE_SIZE(image));

        png_image_finish_read(
            &image, NULL, result.pixels.data(), PNG_IMAGE_SIZE(image) / image.height, NULL);
        if(PNG_IMAGE_FAILED(image)) {
            std::string msg("PNG file reading failed: ");
            msg += image.message;
            throw std::runtime_error(std::move(msg));
        }
    } else {
        std::string msg("Opening a PNG file failed: ");
        msg += image.message;
        throw std::runtime_error(std::move(msg));
    }
    result.w = image.width;
    result.h = image.height;
    png_image_free(&image);

    return result;
}
