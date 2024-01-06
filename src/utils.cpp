// SPDX-License-Identifier: Apache-2.0
// Copyright 2022-2024 Jussi Pakkanen

#include <utils.hpp>
#include <zlib.h>
#include <cassert>
#include <cstring>
#ifdef _WIN32
#include <time.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <fmt/core.h>
#include <memory>
#include <random>

namespace capypdf {

namespace {

struct UtfDecodeStep {
    uint32_t byte1_data_mask;
    uint32_t num_subsequent_bytes;
};

bool is_valid_uf8_character(std::string_view input, size_t cur, const UtfDecodeStep &par) {
    const uint32_t byte1 = uint32_t((unsigned char)input[cur]);
    const uint32_t subsequent_header_mask = 0b011000000;
    const uint32_t subsequent_header_value = 0b10000000;
    const uint32_t subsequent_data_mask = 0b111111;
    const uint32_t subsequent_num_data_bits = 6;

    if(cur + par.num_subsequent_bytes >= input.size()) {
        return false;
    }
    uint32_t unpacked = byte1 & par.byte1_data_mask;
    for(uint32_t i = 0; i < par.num_subsequent_bytes; ++i) {
        unpacked <<= subsequent_num_data_bits;
        const uint32_t subsequent = uint32_t((unsigned char)input[cur + 1 + i]);
        if((subsequent & subsequent_header_mask) != subsequent_header_value) {
            return false;
        }
        assert((unpacked & subsequent_data_mask) == 0);
        unpacked |= subsequent & subsequent_data_mask;
    }

    return true;
}

void append_glyph_to_utf16be(uint32_t glyph, std::vector<uint16_t> &u16buf) {
    if(glyph < 0x10000) {
        u16buf.push_back((uint16_t)glyph);
    } else {
        const auto reduced = glyph - 0x10000;
        const auto high_surrogate = (reduced >> 10) + 0xD800;
        const auto low_surrogate = (reduced & 0b1111111111) + 0xDC00;
        u16buf.push_back((uint16_t)high_surrogate);
        u16buf.push_back((uint16_t)low_surrogate);
    }
}

bool needs_quoting(const unsigned char c) {
    if(c < '!' || c > '~') {
        return true;
    }
    if(c == '#' || c == '(' || c == ')' || c == ' ' || c == '/') {
        return true;
    }
    return false;
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
        RETERR(CompressionFailure);
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
    if(!std::filesystem::is_regular_file(fname)) {
        RETERR(FileDoesNotExist);
    }

    return load_file(fname.string().c_str());
}

rvoe<std::string> load_file(FILE *f) {
    fseek(f, 0, SEEK_END);
    auto fsize = (size_t)ftell(f);
    std::string contents(fsize, '\0');
    fseek(f, 0, SEEK_SET);
    if(fread(contents.data(), 1, fsize, f) != fsize) {
        perror(nullptr);
        RETERR(FileReadError);
    }
    return contents;
}

void write_file(const char *ofname, const char *buf, size_t bufsize) {
    FILE *f = fopen(ofname, "w");
    if(!f) {
        std::abort();
    }
    if(fwrite(buf, 1, bufsize, f) != bufsize) {
        std::abort();
    }
    fclose(f);
}

std::string utf8_to_pdfmetastr(const u8string &input) {
    // For now put everything into UTF-16 bracketstrings.
    std::string encoded = "<FEFF";

    //    auto u16buf = glyphs_to_utf16be(glyphs);
    auto bi = std::back_inserter(encoded);
    std::vector<uint16_t> u16buf;
    for(const auto codepoint : input) {
        u16buf.clear();
        append_glyph_to_utf16be(codepoint, u16buf);
        for(const auto &u16 : u16buf) {
            const auto *lower_byte = (const unsigned char *)&u16;
            const auto *upper_byte = lower_byte + 1;
            fmt::format_to(bi, "{:02X}{:02X}", *upper_byte, *lower_byte);
        }
    }
    encoded += '>';
    return encoded;
}

bool is_valid_utf8(std::string_view input) {
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
            return false;
        }
        if(!is_valid_uf8_character(input, i, par)) {
            return false;
        }
        i += par.num_subsequent_bytes;
    }
    return true;
}

std::string current_date_string() {
    const int bufsize = 128;
    char buf[bufsize];
    time_t timepoint;
    struct tm utctime;
    if(getenv("SOURCE_DATE_EPOCH")) {
        timepoint = strtol(getenv("SOURCE_DATE_EPOCH"), nullptr, 10);
    } else {
        timepoint = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
#ifdef _WIN32
    if(gmtime_s(&utctime, &timepoint) != 0) {
#else
    if(gmtime_r(&timepoint, &utctime) == nullptr) {
#endif
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

// As in PDF 2.0 spec 7.3.5
std::string bytes2pdfstringliteral(std::string_view raw) {
    std::string result;
    char buf[10];
    result.reserve(raw.size() + 1);
    result.push_back('/');
    for(const unsigned char c : raw) {
        if(needs_quoting(c)) {
            result += '#';
            if(snprintf(buf, 10, "%02x", (unsigned int)c) != 2) {
                std::abort();
            }
            result += buf;
        } else {
            result += c;
        }
    }
    return result;
}

std::string create_trailer_id() {
    int num_bytes = 16;
    std::string msg;
    msg.reserve(num_bytes * 2 + 2);
    auto app = std::back_inserter(msg);
    msg.push_back('<');
    std::random_device r;
    std::default_random_engine gen(r());
    std::uniform_int_distribution<int> dist(0, 255);
    for(int i = 0; i < num_bytes; ++i) {
        fmt::format_to(app, "{:02X}", (unsigned char)dist(gen));
    }
    msg.push_back('>');
    return msg;
}

} // namespace capypdf
