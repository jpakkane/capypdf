#pragma once

#include <cstdint>
#include <string>

struct rgb_image {
    int32_t w;
    int32_t h;
    std::string pixels;
};

rgb_image load_image_file(const char *fname);
