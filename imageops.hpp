#pragma once

#include <cstdint>
#include <string>
#include <optional>
struct rgb_image {
    int32_t w;
    int32_t h;
    std::string pixels;
    std::optional<std::string> alpha;
};

rgb_image load_image_file(const char *fname);
