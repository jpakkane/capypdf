/*
 * Copyright 2022-2023 Jussi Pakkanen
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

#include <utils.hpp>
#include <zlib.h>
#include <cassert>
#include <cstring>
#include <sys/time.h>

#include <fmt/core.h>
#include <memory>

namespace A4PDF {

namespace {

struct UtfDecodeStep {
    uint32_t byte1_data_mask;
    uint32_t num_subsequent_bytes;
};

rvoe<uint32_t> unpack_one(std::string_view input, size_t cur, const UtfDecodeStep &par) {
    const uint32_t byte1 = uint32_t((unsigned char)input[cur]);
    const uint32_t subsequent_header_mask = 0b011000000;
    const uint32_t subsequent_header_value = 0b10000000;
    const uint32_t subsequent_data_mask = 0b111111;
    const uint32_t subsequent_num_data_bits = 6;

    if(cur + par.num_subsequent_bytes >= input.size()) {
        RETERR(BadUtf8);
    }
    uint32_t unpacked = byte1 & par.byte1_data_mask;
    for(uint32_t i = 0; i < par.num_subsequent_bytes; ++i) {
        unpacked <<= subsequent_num_data_bits;
        const uint32_t subsequent = uint32_t((unsigned char)input[cur + 1 + i]);
        if((subsequent & subsequent_header_mask) != subsequent_header_value) {
            RETERR(BadUtf8);
        }
        assert((unpacked & subsequent_data_mask) == 0);
        unpacked |= subsequent & subsequent_data_mask;
    }

    return unpacked;
}

std::vector<uint16_t> glyphs_to_utf16be(const std::vector<uint32_t> &glyphs) {
    std::vector<uint16_t> u16buf;
    u16buf.reserve(glyphs.size());
    for(const auto &g : glyphs) {
        if(g < 0x10000) {
            u16buf.push_back((uint16_t)g);
        } else {
            const auto reduced = g - 0x10000;
            const auto high_surrogate = (reduced >> 10) + 0xD800;
            const auto low_surrogate = (reduced & 0b1111111111) + 0xDC00;
            u16buf.push_back((uint16_t)high_surrogate);
            u16buf.push_back((uint16_t)low_surrogate);
        }
    }
    return u16buf;
}

} // namespace

rvoe<std::string> flate_compress(std::string_view data) {
    std::string compressed;
    const int CHUNK = 1024 * 1024;
    std::string buf;
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    auto ret = deflateInit(&strm, Z_BEST_COMPRESSION);
    if(ret != Z_OK) {
        RETERR(CompressionFailure);
    }
    std::unique_ptr<z_stream, int (*)(z_stream *)> zcloser(&strm, deflateEnd);
    strm.avail_in = data.size();
    strm.next_in = (Bytef *)(data.data()); // Very unsafe.

    do {
        buf.resize(CHUNK);
        strm.avail_out = CHUNK;
        strm.next_out = (Bytef *)buf.data();
        ret = deflate(&strm, Z_FINISH); /* no bad return value */
        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
        int write_size = CHUNK - strm.avail_out;
        assert(write_size <= (int)buf.size());
        buf.resize(write_size);
        compressed += buf;
    } while(strm.avail_out == 0);
    if(strm.avail_in != 0) { /* all input will be used */
        RETERR(CompressionFailure);
    }
    /* done when last data in file processed */
    if(ret != Z_STREAM_END) {
        std::unexpected(ErrorCode::CompressionFailure);
    }

    return std::move(compressed);
}

rvoe<std::string> load_file(const char *fname) {
    FILE *f = fopen(fname, "rb");
    if(!f) {
        perror(nullptr);
        RETERR(CouldNotOpenFile);
    }
    std::unique_ptr<FILE, int (*)(FILE *)> fcloser(f, fclose);
    return load_file(f);
}

rvoe<std::string> load_file(const std::filesystem::path &fname) {
    return load_file(fname.string().c_str());
}

rvoe<std::string> load_file(FILE *f) {
    fseek(f, 0, SEEK_END);
    auto fsize = (size_t)ftell(f);
    std::string contents(fsize, '\0');
    fseek(f, 0, SEEK_SET);
    if(fread(contents.data(), 1, fsize, f) != fsize) {
        fclose(f);
        perror(nullptr);
        RETERR(FileReadError);
    }
    return contents;
}

rvoe<std::string> utf8_to_pdfmetastr(std::string_view input) {
    ERC(glyphs, utf8_to_glyphs(input));
    // For now put everything into UTF-16 bracketstrings.
    std::string encoded = "<FEFF";

    auto u16buf = glyphs_to_utf16be(glyphs);
    auto bi = std::back_inserter(encoded);
    for(const auto &u16 : u16buf) {
        const auto *lower_byte = (const unsigned char *)&u16;
        const auto *upper_byte = lower_byte + 1;
        fmt::format_to(bi, "{:02X}{:02X}", *upper_byte, *lower_byte);
    }
    encoded += '>';
    return std::move(encoded);
}

rvoe<std::vector<uint32_t>> utf8_to_glyphs(std::string_view input) {
    std::vector<uint32_t> glyphs;
    UtfDecodeStep par;
    // clang-format off
    const uint32_t twobyte_header_mask    = 0b11100000;
    const uint32_t twobyte_header_value   = 0b11000000;
    const uint32_t threebyte_header_mask  = 0b11110000;
    const uint32_t threebyte_header_value = 0b11100000;
    const uint32_t fourbyte_header_mask   = 0b11111000;
    const uint32_t fourbyte_header_value  = 0b11110000;
    // clang-format on
    for(size_t i = 0; i < input.size(); ++i) {
        const uint32_t code = uint32_t((unsigned char)input[i]);
        if(code < 0x80) {
            glyphs.push_back(code);
            continue;
        } else if((code & twobyte_header_mask) == twobyte_header_value) {
            par.byte1_data_mask = 0b11111;
            par.num_subsequent_bytes = 1;
        } else if((code & threebyte_header_mask) == threebyte_header_value) {
            par.byte1_data_mask = 0b1111;
            par.num_subsequent_bytes = 2;
        } else if((code & fourbyte_header_mask) == fourbyte_header_value) {
            par.byte1_data_mask = 0b111;
            par.num_subsequent_bytes = 3;
        } else {
            RETERR(BadUtf8);
        }
        ERC(unpacked, unpack_one(input, i, par));
        glyphs.push_back(unpacked);
        i += par.num_subsequent_bytes;
    }
    return glyphs;
}

std::string current_date_string() {
    const int bufsize = 128;
    char buf[bufsize];
    time_t timepoint;
    struct tm utctime;
    struct timespec tp;
    if(getenv("SOURCE_DATE_EPOCH")) {
        timepoint = strtol(getenv("SOURCE_DATE_EPOCH"), nullptr, 10);
    } else {
        if(clock_gettime(CLOCK_REALTIME, &tp) != 0) {
            // This is so rare we're just going to ignore it.
            perror(nullptr);
            std::abort();
        }
        timepoint = tp.tv_sec;
    }
    if(gmtime_r(&timepoint, &utctime) == nullptr) {
        perror(nullptr);
        std::abort();
    }
    strftime(buf, bufsize, "(D:%Y%m%d%H%M%SZ)", &utctime);
    return std::string(buf);
}

std::string pdfstring_quote(std::string_view raw_string) {
    std::string result;
    result.reserve(raw_string.size() * 2 + 2);
    result.push_back('(');
    for(const char c : raw_string) {
        switch(c) {
        case '(':
        case ')':
        case '\\':
            result.push_back('\\');
            break;
        default:
            break;
        }
        result.push_back(c);
    }
    result.push_back(')');
    return result;
}

bool is_ascii(std::string_view text) {
    for(const auto c : text) {
        auto ci = int32_t((unsigned char)c);
        if(ci > 127) {
            return false;
        }
    }
    return true;
}

} // namespace A4PDF
